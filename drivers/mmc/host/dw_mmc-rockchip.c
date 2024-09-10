// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2014, Fuzhou Rockchip Electronics Co., Ltd
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/of_address.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/pm_runtime.h>
#include <linux/rockchip/cpu.h>
#include <linux/slab.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"

#define RK3288_CLKGEN_DIV       2

struct dw_mci_rockchip_priv_data {
	struct clk		*drv_clk;
	struct clk		*sample_clk;
	int			default_sample_phase;
	int			num_phases;
	bool			use_v2_tuning;
	int			last_degree;
	u32			f_min;
};

static void dw_mci_rk3288_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
	struct dw_mci_rockchip_priv_data *priv = host->priv;
	int ret;
	unsigned int cclkin;
	u32 bus_hz;

	if (ios->clock == 0)
		return;

	/*
	 * cclkin: source clock of mmc controller
	 * bus_hz: card interface clock generated by CLKGEN
	 * bus_hz = cclkin / RK3288_CLKGEN_DIV
	 * ios->clock = (div == 0) ? bus_hz : (bus_hz / (2 * div))
	 *
	 * Note: div can only be 0 or 1, but div must be set to 1 for eMMC
	 * DDR52 8-bit mode.
	 */
	if (ios->clock < priv->f_min) {
		ios->clock = priv->f_min;
		host->slot->clock = ios->clock;
	}

	if (ios->bus_width == MMC_BUS_WIDTH_8 &&
	    ios->timing == MMC_TIMING_MMC_DDR52)
		cclkin = 2 * ios->clock * RK3288_CLKGEN_DIV;
	else
		cclkin = ios->clock * RK3288_CLKGEN_DIV;

	ret = clk_set_rate(host->ciu_clk, cclkin);
	if (ret)
		dev_warn(host->dev, "failed to set rate %uHz\n", ios->clock);

	bus_hz = clk_get_rate(host->ciu_clk) / RK3288_CLKGEN_DIV;
	if (bus_hz != host->bus_hz) {
		host->bus_hz = bus_hz;
		/* force dw_mci_setup_bus() */
		host->current_speed = 0;
	}

	/* Make sure we use phases which we can enumerate with */
	if (!IS_ERR(priv->sample_clk) && ios->timing <= MMC_TIMING_SD_HS)
		clk_set_phase(priv->sample_clk, priv->default_sample_phase);

	/*
	 * Set the drive phase offset based on speed mode to achieve hold times.
	 *
	 * NOTE: this is _not_ a value that is dynamically tuned and is also
	 * _not_ a value that will vary from board to board.  It is a value
	 * that could vary between different SoC models if they had massively
	 * different output clock delays inside their dw_mmc IP block (delay_o),
	 * but since it's OK to overshoot a little we don't need to do complex
	 * calculations and can pick values that will just work for everyone.
	 *
	 * When picking values we'll stick with picking 0/90/180/270 since
	 * those can be made very accurately on all known Rockchip SoCs.
	 *
	 * Note that these values match values from the DesignWare Databook
	 * tables for the most part except for SDR12 and "ID mode".  For those
	 * two modes the databook calculations assume a clock in of 50MHz.  As
	 * seen above, we always use a clock in rate that is exactly the
	 * card's input clock (times RK3288_CLKGEN_DIV, but that gets divided
	 * back out before the controller sees it).
	 *
	 * From measurement of a single device, it appears that delay_o is
	 * about .5 ns.  Since we try to leave a bit of margin, it's expected
	 * that numbers here will be fine even with much larger delay_o
	 * (the 1.4 ns assumed by the DesignWare Databook would result in the
	 * same results, for instance).
	 */
	if (!IS_ERR(priv->drv_clk)) {
		int phase;

		/*
		 * In almost all cases a 90 degree phase offset will provide
		 * sufficient hold times across all valid input clock rates
		 * assuming delay_o is not absurd for a given SoC.  We'll use
		 * that as a default.
		 */
		phase = 90;

		switch (ios->timing) {
		case MMC_TIMING_MMC_DDR52:
			/*
			 * Since clock in rate with MMC_DDR52 is doubled when
			 * bus width is 8 we need to double the phase offset
			 * to get the same timings.
			 */
			if (ios->bus_width == MMC_BUS_WIDTH_8)
				phase = 180;
			break;
		case MMC_TIMING_UHS_SDR104:
		case MMC_TIMING_MMC_HS200:
			/*
			 * In the case of 150 MHz clock (typical max for
			 * Rockchip SoCs), 90 degree offset will add a delay
			 * of 1.67 ns.  That will meet min hold time of .8 ns
			 * as long as clock output delay is < .87 ns.  On
			 * SoCs measured this seems to be OK, but it doesn't
			 * hurt to give margin here, so we use 180.
			 */
			phase = 180;
			break;
		}

		clk_set_phase(priv->drv_clk, phase);
	}
}

#define TUNING_ITERATION_TO_PHASE(i, num_phases) \
		(DIV_ROUND_UP((i) * 360, num_phases))

static int dw_mci_v2_execute_tuning(struct dw_mci_slot *slot, u32 opcode)
{
	struct dw_mci *host = slot->host;
	struct dw_mci_rockchip_priv_data *priv = host->priv;
	struct mmc_host *mmc = slot->mmc;
	u32 degrees[4] = {0, 90, 180, 270}, degree;
	int i;
	static bool inherit = true;

	if (inherit) {
		inherit = false;
		i = clk_get_phase(priv->sample_clk) / 90;
		degree = degrees[i];
		goto done;
	}

	/*
	 * v2 only support 4 degrees in theory.
	 * First we inherit sample phases from firmware, which should
	 * be able work fine, at least in the first place.
	 * If retune is needed, we search forward to pick the last
	 * one phase from degree list and loop around until we get one.
	 * It's impossible all 4 fixed phase won't be able to work.
	 */
	for (i = 0; i < ARRAY_SIZE(degrees); i++) {
		degree = degrees[i] + priv->last_degree + 90;
		degree = degree % 360;
		clk_set_phase(priv->sample_clk, degree);
		if (mmc_send_tuning(mmc, opcode, NULL)) {
			/*
			 * Tuning error, the phase is a bad phase,
			 * then try using the calculated best phase.
			 */
			dev_info(host->dev, "V2 tuned phase to %d error, try the best phase\n", degree);
			degree = (degree + 180) % 360;
			clk_set_phase(priv->sample_clk, degree);
			if (!mmc_send_tuning(mmc, opcode, NULL))
				break;
		}
	}

	if (i == ARRAY_SIZE(degrees)) {
		dev_warn(host->dev, "V2 All phases bad!");
		return -EIO;
	}

done:
	dev_info(host->dev, "V2 Successfully tuned phase to %d\n", degree);
	priv->last_degree = degree;
	return 0;
}

static int dw_mci_rk3288_execute_tuning(struct dw_mci_slot *slot, u32 opcode)
{
	struct dw_mci *host = slot->host;
	struct dw_mci_rockchip_priv_data *priv = host->priv;
	struct mmc_host *mmc = slot->mmc;
	int ret = 0;
	int i;
	bool v, prev_v = 0, first_v;
	struct range_t {
		int start;
		int end; /* inclusive */
	};
	struct range_t *ranges;
	unsigned int range_count = 0;
	int longest_range_len = -1;
	int longest_range = -1;
	int middle_phase, real_middle_phase;

	if (IS_ERR(priv->sample_clk)) {
		dev_err(host->dev, "Tuning clock (sample_clk) not defined.\n");
		return -EIO;
	}

	if (priv->use_v2_tuning) {
		if (!dw_mci_v2_execute_tuning(slot, opcode))
			return 0;
		/* Otherwise we continue using fine tuning */
	}

	ranges = kmalloc_array(priv->num_phases / 2 + 1,
			       sizeof(*ranges), GFP_KERNEL);
	if (!ranges)
		return -ENOMEM;

	/* Try each phase and extract good ranges */
	for (i = 0; i < priv->num_phases; ) {
		/* Cannot guarantee any phases larger than 270 would work well */
		if (TUNING_ITERATION_TO_PHASE(i, priv->num_phases) > 270)
			break;
		clk_set_phase(priv->sample_clk,
			      TUNING_ITERATION_TO_PHASE(i, priv->num_phases));

		v = !mmc_send_tuning(mmc, opcode, NULL);

		if (i == 0)
			first_v = v;

		if ((!prev_v) && v) {
			range_count++;
			ranges[range_count-1].start = i;
		}
		if (v) {
			ranges[range_count-1].end = i;
			i++;
		} else if (i == priv->num_phases - 1) {
			/* No extra skipping rules if we're at the end */
			i++;
		} else {
			/*
			 * No need to check too close to an invalid
			 * one since testing bad phases is slow.  Skip
			 * 20 degrees.
			 */
			i += DIV_ROUND_UP(20 * priv->num_phases, 360);

			/* Always test the last one */
			if (i >= priv->num_phases)
				i = priv->num_phases - 1;
		}

		prev_v = v;
	}

	if (range_count == 0) {
		dev_warn(host->dev, "All phases bad!");
		ret = -EIO;
		goto free;
	}

	/* wrap around case, merge the end points */
	if ((range_count > 1) && first_v && v) {
		ranges[0].start = ranges[range_count-1].start;
		range_count--;
	}

	if (ranges[0].start == 0 && ranges[0].end == priv->num_phases - 1) {
		clk_set_phase(priv->sample_clk, priv->default_sample_phase);
		dev_info(host->dev, "All phases work, using default phase %d.",
			 priv->default_sample_phase);
		goto free;
	}

	/* Find the longest range */
	for (i = 0; i < range_count; i++) {
		int len = (ranges[i].end - ranges[i].start + 1);

		if (len < 0)
			len += priv->num_phases;

		if (longest_range_len < len) {
			longest_range_len = len;
			longest_range = i;
		}

		dev_dbg(host->dev, "Good phase range %d-%d (%d len)\n",
			TUNING_ITERATION_TO_PHASE(ranges[i].start,
						  priv->num_phases),
			TUNING_ITERATION_TO_PHASE(ranges[i].end,
						  priv->num_phases),
			len
		);
	}

	dev_dbg(host->dev, "Best phase range %d-%d (%d len)\n",
		TUNING_ITERATION_TO_PHASE(ranges[longest_range].start,
					  priv->num_phases),
		TUNING_ITERATION_TO_PHASE(ranges[longest_range].end,
					  priv->num_phases),
		longest_range_len
	);

	middle_phase = ranges[longest_range].start + longest_range_len / 2;
	middle_phase %= priv->num_phases;
	real_middle_phase = TUNING_ITERATION_TO_PHASE(middle_phase, priv->num_phases);

	/*
	 * Since we cut out 270 ~ 360, the original algorithm
	 * still rolling ranges before and after 270 together
	 * in some corner cases, we should adjust it to avoid
	 * using any middle phase located between 270 and 360.
	 * By calculatiion, it happends due to the bad phases
	 * lay between 90 ~ 180. So others are all fine to chose.
	 * Pick 270 is a better choice in those cases. In case of
	 * bad phases exceed 180, the middle phase of rollback
	 * would be bigger than 315, so we chose 360.
	 */
	if (real_middle_phase > 270) {
		if (real_middle_phase < 315)
			real_middle_phase = 270;
		else
			real_middle_phase = 360;
	}

	dev_info(host->dev, "Successfully tuned phase to %d\n",
		 real_middle_phase);

	clk_set_phase(priv->sample_clk, real_middle_phase);

free:
	kfree(ranges);
	return ret;
}

static int dw_mci_rk3288_parse_dt(struct dw_mci *host)
{
	struct device_node *np = host->dev->of_node;
	struct dw_mci_rockchip_priv_data *priv;

	priv = devm_kzalloc(host->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/*
	 * RK356X SoCs only support 375KHz for ID mode, so any clk request
	 * that less than 1.6MHz(2 * 400KHz * RK3288_CLKGEN_DIV) should be
	 * wrapped  into 375KHz
	 */
	if (of_device_is_compatible(host->dev->of_node,
				    "rockchip,rk3568-dw-mshc"))
		priv->f_min = 375000;
	else
		priv->f_min = 100000;

	if (of_property_read_u32(np, "rockchip,desired-num-phases",
					&priv->num_phases))
		priv->num_phases = 360;

	if (of_property_read_u32(np, "rockchip,default-sample-phase",
					&priv->default_sample_phase))
		priv->default_sample_phase = 0;

	if (of_property_read_bool(np, "rockchip,use-v2-tuning"))
		priv->use_v2_tuning = true;

	priv->drv_clk = devm_clk_get(host->dev, "ciu-drive");
	if (IS_ERR(priv->drv_clk))
		dev_dbg(host->dev, "ciu-drive not available\n");

	priv->sample_clk = devm_clk_get(host->dev, "ciu-sample");
	if (IS_ERR(priv->sample_clk))
		dev_dbg(host->dev, "ciu-sample not available\n");

	host->priv = priv;

	return 0;
}

static int dw_mci_rockchip_init(struct dw_mci *host)
{
	/* It is slot 8 on Rockchip SoCs */
	host->sdio_id0 = 8;

	if (of_device_is_compatible(host->dev->of_node,
				    "rockchip,rk3288-dw-mshc"))
		host->bus_hz /= RK3288_CLKGEN_DIV;

	if (of_device_is_compatible(host->dev->of_node,
				    "rockchip,rv1106-dw-mshc") &&
	    rockchip_get_cpu_version() == 0 &&
	    !strcmp(dev_name(host->dev), "ffaa0000.mmc")) {
		if (device_property_read_bool(host->dev, "no-sd")) {
			dev_err(host->dev, "Invalid usage, should be SD card only\n");
			return -EINVAL;
		}

		host->is_rv1106_sd = true;
		dev_info(host->dev, "is rv1106 sd\n");
	}

	host->need_xfer_timer = true;
	return 0;
}

/* Common capabilities of RK3288 SoC */
static unsigned long dw_mci_rk3288_dwmmc_caps[4] = {
	MMC_CAP_CMD23,
	MMC_CAP_CMD23,
	MMC_CAP_CMD23,
	MMC_CAP_CMD23,
};

static const struct dw_mci_drv_data rk2928_drv_data = {
	.init			= dw_mci_rockchip_init,
};

static const struct dw_mci_drv_data rk3288_drv_data = {
	.caps			= dw_mci_rk3288_dwmmc_caps,
	.num_caps		= ARRAY_SIZE(dw_mci_rk3288_dwmmc_caps),
	.set_ios		= dw_mci_rk3288_set_ios,
	.execute_tuning		= dw_mci_rk3288_execute_tuning,
	.parse_dt		= dw_mci_rk3288_parse_dt,
	.init			= dw_mci_rockchip_init,
};

static const struct of_device_id dw_mci_rockchip_match[] = {
	{ .compatible = "rockchip,rk2928-dw-mshc",
		.data = &rk2928_drv_data },
	{ .compatible = "rockchip,rk3288-dw-mshc",
		.data = &rk3288_drv_data },
	{},
};
MODULE_DEVICE_TABLE(of, dw_mci_rockchip_match);

static int dw_mci_rockchip_probe(struct platform_device *pdev)
{
	const struct dw_mci_drv_data *drv_data;
	const struct of_device_id *match;
	int ret;
	bool use_rpm = true;

	if (!pdev->dev.of_node)
		return -ENODEV;

	if ((!device_property_read_bool(&pdev->dev, "non-removable") &&
	     !device_property_read_bool(&pdev->dev, "cd-gpios")) ||
	    (device_property_read_bool(&pdev->dev, "no-sd") &&
	     device_property_read_bool(&pdev->dev, "no-mmc")))
		use_rpm = false;

	match = of_match_node(dw_mci_rockchip_match, pdev->dev.of_node);
	drv_data = match->data;

	/*
	 * increase rpm usage count in order to make
	 * pm_runtime_force_resume calls rpm resume callback
	 */
	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);

	if (use_rpm) {
		pm_runtime_enable(&pdev->dev);
		pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
		pm_runtime_use_autosuspend(&pdev->dev);
	}

	ret = dw_mci_pltfm_register(pdev, drv_data);
	if (ret) {
		if (use_rpm) {
			pm_runtime_disable(&pdev->dev);
			pm_runtime_set_suspended(&pdev->dev);
		}
		pm_runtime_put_noidle(&pdev->dev);
		return ret;
	}

	if (use_rpm)
		pm_runtime_put_autosuspend(&pdev->dev);

	return 0;
}

static int dw_mci_rockchip_remove(struct platform_device *pdev)
{
	pm_runtime_get_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);

	return dw_mci_pltfm_remove(pdev);
}

static const struct dev_pm_ops dw_mci_rockchip_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(dw_mci_runtime_suspend,
			   dw_mci_runtime_resume,
			   NULL)
};

static struct platform_driver dw_mci_rockchip_pltfm_driver = {
	.probe		= dw_mci_rockchip_probe,
	.remove		= dw_mci_rockchip_remove,
	.driver		= {
		.name		= "dwmmc_rockchip",
		.probe_type	= PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table	= dw_mci_rockchip_match,
		.pm		= &dw_mci_rockchip_dev_pm_ops,
	},
};

module_platform_driver(dw_mci_rockchip_pltfm_driver);

MODULE_AUTHOR("Addy Ke <addy.ke@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip Specific DW-MSHC Driver Extension");
MODULE_ALIAS("platform:dwmmc_rockchip");
MODULE_LICENSE("GPL v2");
