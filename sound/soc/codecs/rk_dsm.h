/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Rockchip Audio Delta-sigma Digital Converter driver
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd
 *
 */

#ifndef _RK_DSM_H
#define _RK_DSM_H

#define DACVUCTL		0x0000
#define DACVUCTIME		0x0004
#define DACDIGEN		0x0008
#define DACCLKCTRL		0x000c
#define DACINT_DIV		0x0014
#define DACSCLKRXINT_DIV	0x0020
#define DACDSM_DIV		0x0024
#define DACDSM_CTRL		0x0028
#define DACCFG1			0x0044
#define DACMUTE			0x0048
#define DACMUTEST		0x004c
#define DACVOLL0		0x0050
#define DACVOLL1		0x0054
#define DACVOLL2		0x0058
#define DACVOLL3		0x005c
#define DACVOLR0		0x0060
#define DACVOLR1		0x0064
#define DACVOLR2		0x0068
#define DACVOLR3		0x006c
#define DACVOGP			0x0070
#define DACRVOLL0		0x0074
#define DACRVOLL1		0x0078
#define DACRVOLL2		0x007c
#define DACRVOLL3		0x0080
#define DACRVOLR0		0x0084
#define DACRVOLR1		0x0088
#define DACRVOLR2		0x008c
#define DACRVOLR3		0x0090
#define DACLMT0			0x0094
#define DACLMT1			0x0098
#define DACLMT2			0x009c
#define DACMIXCTRLL		0x00a0
#define DACMIXCTRLR		0x00a4
#define DACHPF			0x00a8
#define I2S_RXCR0		0x010c
#define I2S_RXCR1		0x0110
#define I2S_CKR0		0x0114
#define I2S_CKR1		0x0118
#define I2S_XFER		0x011c
#define I2S_CLR			0x0120
#define VERSION			0x0140

/* DACDIGEN */
#define DSM_DACDIGEN_DACEN_L0R1_MASK		BIT(0)
#define DSM_DACDIGEN_DACEN_L0R1_EN		BIT(0)
#define DSM_DACDIGEN_DACEN_L0R1_DIS		0
#define DSM_DACDIGEN_DAC_GLBEN_MASK		BIT(4)
#define DSM_DACDIGEN_DAC_GLBEN_EN		BIT(4)
#define DSM_DACDIGEN_DAC_GLBEN_DIS		0
/* DACCLKCTRL */
#define DSM_DACCLKCTRL_DAC_MODE_ATTENU_MASK	BIT(0)
#define DSM_DACCLKCTRL_DAC_MODE_ATTENU_EN	BIT(0)
#define DSM_DACCLKCTRL_DAC_MODE_ATTENU_DIS	0
#define DSM_DACCLKCTRL_DAC_SYNC_STATUS_MASK	BIT(1)
#define DSM_DACCLKCTRL_DAC_SYNC_STATUS_DONE	0
#define DSM_DACCLKCTRL_DAC_SYNC_ENA_MASK	BIT(2)
#define DSM_DACCLKCTRL_DAC_SYNC_ENA_EN		BIT(2)
#define DSM_DACCLKCTRL_DAC_SYNC_ENA_DIS		0
#define DSM_DACCLKCTRL_CKE_BCLKRX_MASK		BIT(3)
#define DSM_DACCLKCTRL_CKE_BCLKRX_EN		BIT(3)
#define DSM_DACCLKCTRL_CKE_BCLKRX_DIS		0
#define DSM_DACCLKCTRL_I2SRX_CKE_MASK		BIT(4)
#define DSM_DACCLKCTRL_I2SRX_CKE_EN		BIT(4)
#define DSM_DACCLKCTRL_I2SRX_CKE_DIS		0
#define DSM_DACCLKCTRL_DAC_CKE_MASK		BIT(5)
#define DSM_DACCLKCTRL_DAC_CKE_EN		BIT(5)
#define DSM_DACCLKCTRL_DAC_CKE_DIS		0
/* DACINT_DIV */
#define DSM_DACINT_DIV_INT_DIV_CON_MASK		GENMASK(7, 0)
#define DSM_DACINT_DIV_INT_DIV_CON(x)		((x) - 1)
/* DACSCLKRXINT_DIV */
#define DSM_DACSCLKRXINT_DIV_SCKRXDIV_MASK	GENMASK(7, 0)
#define DSM_DACSCLKRXINT_DIV_SCKRXDIV(x)	((x) - 1)
/* DACDSM_DIV */
#define DSM_DACDSM_DIV_AUDIO_DSM_DIV_MASK	GENMASK(7, 0)
#define DSM_DACDSM_DIV_AUDIO_DSM_DIV(x)		((x) - 1)
/* DACDSM_CTRL */
#define DSM_DACDSM_CTRL_DITH_SEL_MASK		GENMASK(2, 0)
#define DSM_DACDSM_CTRL_DITH_SEL(x)		(x)
#define DSM_DACDSM_CTRL_DSM_EN_MASK		BIT(3)
#define DSM_DACDSM_CTRL_DSM_EN			BIT(3)
#define DSM_DACDSM_CTRL_DSM_DIS			0
#define DSM_DACDSM_CTRL_DSM_MODE_MASK		GENMASK(5, 4)
#define DSM_DACDSM_CTRL_DSM_MODE_1		(0x2 << 4)
#define DSM_DACDSM_CTRL_DSM_MODE_0		(0x1 << 4)
#define DSM_DACDSM_CTRL_DSM_MODE_DAC		(0x0 << 4)
#define DSM_DACDSM_CTRL_DSM_MODE_CKE_MASK	BIT(6)
#define DSM_DACDSM_CTRL_DSM_MODE_CKE_EN		BIT(6)
#define DSM_DACDSM_CTRL_DSM_MODE_CKE_DIS	0
/* DACCFG1 */
#define DSM_DACCFG1_DACSRT_MASK			GENMASK(4, 2)
#define DSM_DACCFG1_DACSRT(x)			((x) << 2)
/* DACMUTE */
#define DSM_DACMUTE_DACMT_MASK			BIT(0)
#define DSM_DACMUTE_DACUNMT_MASK		BIT(1)
/* DACVOLL0 */
#define DSM_DACVOLL0_DACLV0_MASK		GENMASK(7, 0)
#define DSM_DACVOLL0_DACLV0(x)			(x)
/* DACVOLR0 */
#define DSM_DACVOLR0_DACRV0_MASK		GENMASK(7, 0)
#define DSM_DACVOLR0_DACRV0(x)			(x)
/* DACVOGP */
#define DSM_DACVOGP_VOLGPL0_MASK		BIT(0)
#define DSM_DACVOGP_VOLGPL0_POS			BIT(0)
#define DSM_DACVOGP_VOLGPL0_NEG			0
#define DSM_DACVOGP_VOLGPR0_MASK		BIT(1)
#define DSM_DACVOGP_VOLGPR0_POS			BIT(1)
#define DSM_DACVOGP_VOLGPR0_NEG			0
/* DACMIXCTRLL */
#define DSM_DACMIXCTRLL_MIXMODE_L0_MASK		GENMASK(1, 0)
#define DSM_DACMIXCTRLL_MIXMODE_L0_LR		2
#define DSM_DACMIXCTRLL_MIXMODE_L0_R		1
#define DSM_DACMIXCTRLL_MIXMODE_L0_L		0
/* DACMIXCTRLR */
#define DSM_DACMIXCTRLR_MIXMODE_R0_MASK		GENMASK(1, 0)
#define DSM_DACMIXCTRLR_MIXMODE_R0_LR		2
#define DSM_DACMIXCTRLR_MIXMODE_R0_L		1
#define DSM_DACMIXCTRLR_MIXMODE_R0_R		0
/* DACHPF */
#define DSM_DACHPF_HPFEN_L0R0_MASK		BIT(0)
#define DSM_DACHPF_HPFEN_L0R0_EN		BIT(0)
#define DSM_DACHPF_HPFCF_MASK			GENMASK(5, 4)
#define DSM_DACHPF_HPFCF_140HZ			(0x3 << 4)
#define DSM_DACHPF_HPFCF_120HZ			(0x2 << 4)
#define DSM_DACHPF_HPFCF_100HZ			(0x1 << 4)
#define DSM_DACHPF_HPFCF_80HZ			(0x0 << 4)
/* I2S_RXCR0 */
#define DSM_I2S_RXCR0_VDW_MASK			GENMASK(4, 0)
#define DSM_I2S_RXCR0_VDW(x)			((x) - 1)
/* I2S_RXCR1 */
#define DSM_I2S_RXCR1_CEX_MASK			BIT(4)
#define DSM_I2S_RXCR1_CEX_EXCHANGE		BIT(4)
#define DSM_I2S_RXCR1_RCSR_MASK			GENMASK(7, 6)
#define DSM_I2S_RXCR1_RCSR_2CH			(0x0 << 6)
/* I2S_CKR0 */
#define DSM_I2S_CKR0_RSD_MASK			GENMASK(3, 2)
#define DSM_I2S_CKR0_RSD_64			(0 << 2)
#define DSM_I2S_CKR0_RSD_128			(1 << 2)
#define DSM_I2S_CKR0_RSD_256			(2 << 2)
/* I2S_CKR1 */
#define DSM_I2S_CKR1_RLP_MASK			BIT(1)
#define DSM_I2S_CKR1_RLP_INVERTED		BIT(1)
#define DSM_I2S_CKR1_RLP_NORMAL			0
#define DSM_I2S_CKR1_CKP_MASK			BIT(2)
#define DSM_I2S_CKR1_CKP_INVERTED		BIT(2)
#define DSM_I2S_CKR1_CKP_NORMAL			0
#define DSM_I2S_CKR1_MSS_MASK			BIT(3)
#define DSM_I2S_CKR1_MSS_MASTER			0
/* I2S_XFER */
#define DSM_I2S_XFER_RXS_MASK			BIT(1)
#define DSM_I2S_XFER_RXS_START			BIT(1)
#define DSM_I2S_XFER_RXS_STOP			0
/* I2S_CLR */
#define DSM_I2S_CLR_RXC_MASK			BIT(1)
#define DSM_I2S_CLR_RXC_CLR			BIT(1)

#endif
