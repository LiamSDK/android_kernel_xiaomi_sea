// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include "clkdbg.h"

#define CLK_MISC_CFG_0_OFS	0x140
#define CLK_DBG_CFG_OFS		0x17C
#define CLK26CALI_0_OFS		0x220
#define CLK26CALI_1_OFS		0x224

static void __iomem *topck_base;
static void __iomem *apmixed_base;
static void __iomem *spm_base;

static DEFINE_SPINLOCK(fmeter_lock);

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	/* CKGEN Part */
	FMCLK(CKGEN,  1, "hd_faxi_ck"),
	FMCLK(CKGEN,  2, "hf_fscp_ck"),
	FMCLK(CKGEN,  3, "hf_fmfg_ck"),
	FMCLK(CKGEN,  4, "f_fcamtg_ck"),
	FMCLK(CKGEN,  5, "f_fcamtg1_ck"),
	FMCLK(CKGEN,  6, "f_fcamtg2_ck"),
	FMCLK(CKGEN,  7, "f_fcamtg3_ck"),
	FMCLK(CKGEN,  8, "f_fcamtg4_ck"),
	FMCLK(CKGEN,  9, "f_fcamtg5_ck"),
	FMCLK(CKGEN, 10, "f_fcamtg6_ck"),
	FMCLK(CKGEN, 11, "f_fuart_ck"),
	FMCLK(CKGEN, 12, "hf_fspi_ck"),
	FMCLK(CKGEN, 13, "hf_fmsdc50_0_hclk_ck"),
	FMCLK(CKGEN, 14, "hf_fmsdc50_0_ck"),
	FMCLK(CKGEN, 15, "hf_fmsdc30_1_ck"),
	FMCLK(CKGEN, 16, "hf_faudio_ck"),
	FMCLK(CKGEN, 17, "hf_faud_intbus_ck"),
	FMCLK(CKGEN, 18, "hf_faud_1_ck"),
	FMCLK(CKGEN, 19, "hf_faud_2_ck"),
	FMCLK(CKGEN, 20, "hf_faud_engen1_ck"),
	FMCLK(CKGEN, 21, "hf_faud_engen2_ck"),
	FMCLK(CKGEN, 22, "f_fdisp_pwm_ck"),
	FMCLK(CKGEN, 23, "hf_sspm_ck"),
	FMCLK(CKGEN, 24, "hf_fdxcc_ck"),
	FMCLK(CKGEN, 25, "hf_fusb_top_ck"),
	FMCLK(CKGEN, 26, "hf_fsrck_ck"),
	FMCLK(CKGEN, 27, "hf_fspm_ck"),
	FMCLK(CKGEN, 28, "hf_fi2c_ck"),
	FMCLK(CKGEN, 29, "f_fpwm_ck"),
	FMCLK(CKGEN, 30, "f_fseninf_ck"),
	FMCLK(CKGEN, 31, "f_fseninf1_ck"),
	FMCLK(CKGEN, 32, "f_fseninf2_ck"),
	FMCLK(CKGEN, 33, "f_fseninf3_ck"),
	FMCLK(CKGEN, 34, "hdf_faes_msdcfde_ck"),
	FMCLK(CKGEN, 35, "f_fpwrap_ulposc_ck"),
	FMCLK(CKGEN, 36, "f_fcamtm_ck"),
	FMCLK(CKGEN, 37, "hf_fvenc_ck"),
	FMCLK(CKGEN, 38, "hf_fcam_ck"),
	FMCLK(CKGEN, 39, "hf_fimg1_ck"),
	FMCLK(CKGEN, 40, "hf_fipe_ck"),
	FMCLK(CKGEN, 41, "hf_dpmaif_ck"),
	FMCLK(CKGEN, 42, "hf_fvdec_ck"),
	FMCLK(CKGEN, 43, "hf_fdisp_ck"),
	FMCLK(CKGEN, 44, "hf_fmdp_ck"),
	FMCLK(CKGEN, 45, "hf_faudio_h_ck"),
	FMCLK(CKGEN, 46, "hf_fufs_ck"),
	FMCLK(CKGEN, 47, "hf_faes_fde_ck"),
	FMCLK(CKGEN, 48, "hf_audiodsp_ck"),
	FMCLK(CKGEN, 49, "hg_fdvfsrc_ck"),
	FMCLK(CKGEN, 50, "hg_fdvfsrc_ck"),
	FMCLK(CKGEN, 51, "hf_dsi_occ_ck"),
	FMCLK(CKGEN, 52, "hf_fspmi_mst_ck"),
	/* ABIST Part */
	FMCLK(ABIST,  7, "AD_CCIPLL_CK"),
	FMCLK(ABIST,  8, "AD_ARMPLL_L_CK"),
	FMCLK(ABIST,  9, "AD_ARMPLL_CK"),
	FMCLK(ABIST, 10, "AD_PLLGP1_TST_CK"),
	FMCLK(ABIST, 11, "AD_MDBPIPLL_CK"),
	FMCLK(ABIST, 12, "AD_MDBRPPLL_CK"),
	FMCLK(ABIST, 13, "AD_MDVDSPPLL_CK"),
	FMCLK(ABIST, 14, "AD_MDMCUPLL_CK"),
	FMCLK(ABIST, 15, "AD_APLL2_CK"),
	FMCLK(ABIST, 16, "AD_APLL1_CK"),
	FMCLK(ABIST, 17, "AD_USB20_192M_CK"),
	FMCLK(ABIST, 19, "AD_CCIPLL_CK"),
	FMCLK(ABIST, 20, "AD_DSI0_LNTC_DSICLK"),
	FMCLK(ABIST, 21, "AD_DSI0_MPPLL_TST_CK"),
	FMCLK(ABIST, 24, "AD_MAINPLL_CK"),
	FMCLK(ABIST, 25, "AD_MDPLL1_FS26M_CK_guide"),
	FMCLK(ABIST, 26, "AD_MFGPLL_CK"),
	FMCLK(ABIST, 27, "AD_MMPLL_CK"),
	FMCLK(ABIST, 28, "AD_ADSPPLL_CK"),
	FMCLK(ABIST, 29, "AD_MPLL_208M_CK"),
	FMCLK(ABIST, 30, "AD_MSDCPLL_CK"),
	FMCLK(ABIST, 34, "AD_ULPOSC2_CK"),
	FMCLK(ABIST, 35, "AD_ULPOSC_CK"),
	FMCLK(ABIST, 36, "AD_UNIVPLL_CK"),
	FMCLK(ABIST, 40, "ad_wbg_dig_bpll_ck"),
	FMCLK(ABIST, 41, "UFS_MP_CLK2FREQ"),
	FMCLK(ABIST, 42, "AD_RCLRPLL_DIV4_CK"),
	FMCLK(ABIST, 43, "AD_RPHYPLL_DIV4_CK"),
	FMCLK(ABIST, 44, "fmem_ck_aft_dsm_ch0"),
	FMCLK(ABIST, 45, "fmem_ck_aft_dsm_ch1"),
	FMCLK(ABIST, 46, "fmem_ck_bfe_dcm_ch0"),
	FMCLK(ABIST, 47, "fmem_ck_bfe_dcm_ch1"),
	FMCLK(ABIST, 48, "mcusys_arm_clk_out_all"),
	{ /* sentinel */ }
};

const struct fmeter_clk *mt6781_get_fmeter_clks(void)
{
	return fclks;
}
EXPORT_SYMBOL_GPL(mt6781_get_fmeter_clks);

static u32 mt6781_get_ckgen_freq(u32 id)
{
	int output = 0, i = 0;
	unsigned long flags;
	u32 temp, clk_dbg_cfg, clk_misc_cfg_0;

	if (!topck_base)
		return 0;

	spin_lock_irqsave(&fmeter_lock, flags);

	clk_dbg_cfg = readl(topck_base + CLK_DBG_CFG_OFS);
	writel((clk_dbg_cfg & 0xFFFFC0FC) | (id << 8) | 0x1,
	       topck_base + CLK_DBG_CFG_OFS);

	clk_misc_cfg_0 = readl(topck_base + CLK_MISC_CFG_0_OFS);
	writel((clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24),
	       topck_base + CLK_MISC_CFG_0_OFS);

	writel(0x1000, topck_base + CLK26CALI_0_OFS);
	writel(0x1010, topck_base + CLK26CALI_0_OFS);

	while (readl(topck_base + CLK26CALI_0_OFS) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}

	if (i == 0) {
		writel(0x0000, topck_base + CLK26CALI_0_OFS);
		writel(0x1000, topck_base + CLK26CALI_0_OFS);
		writel(0x1010, topck_base + CLK26CALI_0_OFS);
		while (readl(topck_base + CLK26CALI_0_OFS) & 0x10) {
			udelay(10);
			i++;
			if (i > 30)
				break;
		}
	}

	temp = readl(topck_base + CLK26CALI_1_OFS) & 0xFFFF;
	output = (temp * 26000) / 1024;

	writel(clk_dbg_cfg, topck_base + CLK_DBG_CFG_OFS);
	writel(clk_misc_cfg_0, topck_base + CLK_MISC_CFG_0_OFS);
	writel(0x0000, topck_base + CLK26CALI_0_OFS);

	spin_unlock_irqrestore(&fmeter_lock, flags);

	if (i > 30)
		return 0;

	return output * 4;
}

static u32 mt6781_get_abist_freq(u32 id)
{
	int output = 0, i = 0;
	unsigned long flags;
	u32 temp, clk_dbg_cfg, clk_misc_cfg_0;

	if (!topck_base)
		return 0;

	spin_lock_irqsave(&fmeter_lock, flags);

	clk_dbg_cfg = readl(topck_base + CLK_DBG_CFG_OFS);
	writel((clk_dbg_cfg & 0xFFC0FFFC) | (id << 16),
	       topck_base + CLK_DBG_CFG_OFS);

	clk_misc_cfg_0 = readl(topck_base + CLK_MISC_CFG_0_OFS);
	writel((clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24),
	       topck_base + CLK_MISC_CFG_0_OFS);

	writel(0x1000, topck_base + CLK26CALI_0_OFS);
	writel(0x1010, topck_base + CLK26CALI_0_OFS);

	while (readl(topck_base + CLK26CALI_0_OFS) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}

	if (i == 0) {
		writel(0x0000, topck_base + CLK26CALI_0_OFS);
		writel(0x1000, topck_base + CLK26CALI_0_OFS);
		writel(0x1010, topck_base + CLK26CALI_0_OFS);
		while (readl(topck_base + CLK26CALI_0_OFS) & 0x10) {
			udelay(10);
			i++;
			if (i > 30)
				break;
		}
	}

	temp = readl(topck_base + CLK26CALI_1_OFS) & 0xFFFF;
	output = (temp * 26000) / 1024;

	writel(clk_dbg_cfg, topck_base + CLK_DBG_CFG_OFS);
	writel(clk_misc_cfg_0, topck_base + CLK_MISC_CFG_0_OFS);
	writel(0x0000, topck_base + CLK26CALI_0_OFS);

	spin_unlock_irqrestore(&fmeter_lock, flags);

	if (i > 30)
		return 0;

	return output * 4;
}

static u32 mt6781_get_abist2_freq(u32 id)
{
	pr_notice("mt6765 not support abist2 func\n");
	return 0;
}

static u32 __maybe_unused mt6781_get_fmeter_freq(u32 id, enum FMETER_TYPE type)
{
	if (type == CKGEN)
		return mt6781_get_ckgen_freq(id);
	else if (type == ABIST)
		return mt6781_get_abist_freq(id);
	else if (type == ABIST_2)
		return mt6781_get_abist2_freq(id);

	return 0;
}

static u32 __maybe_unused mt6781_get_fmeter_id(const char *name)
{
	int i;

	for (i = 0; fclks[i].name != NULL; i++) {
		if (!strcmp(fclks[i].name, name))
			return fclks[i].id;
	}
	return 0;
}

static void __iomem *get_base_from_comp(const char *compatible)
{
	struct device_node *node;
	void __iomem *base = NULL;

	node = of_find_compatible_node(NULL, NULL, compatible);
	if (!node) {
		pr_err("%s can't find compatible node for %s\n",
		       __func__, compatible);
		return NULL;
	}

	base = of_iomap(node, 0);
	of_node_put(node);
	if (!base) {
		pr_err("%s() can't find iomem for %s\n",
		       __func__, compatible);
		return NULL;
	}

	return base;
}

static int clk_fmeter_mt6781_probe(struct platform_device *pdev)
{
	topck_base = get_base_from_comp("mediatek,mt6781-topckgen");
	if (!topck_base)
		topck_base = get_base_from_comp("mediatek,topckgen");

	apmixed_base = get_base_from_comp("mediatek,mt6781-apmixedsys");
	if (!apmixed_base)
		apmixed_base = get_base_from_comp("mediatek,apmixed");

	spm_base = get_base_from_comp("mediatek,mt6781-scpsys");
	if (!spm_base)
		spm_base = get_base_from_comp("mediatek,scpsys");

	if (!topck_base) {
		pr_err("%s can't find base\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static struct platform_driver clk_fmeter_mt6781_drv = {
	.probe = clk_fmeter_mt6781_probe,
	.driver = {
		.name = "clk-fmeter-mt6781",
	},
};

static struct platform_device *clk_fmeter_dev;

static int __init clk_fmeter_init(void)
{
	int ret;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6781",
							 -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev)) {
		pr_err("unable to register clk-fmeter device\n");
		return PTR_ERR(clk_fmeter_dev);
	}

	ret = platform_driver_register(&clk_fmeter_mt6781_drv);
	if (ret) {
		platform_device_unregister(clk_fmeter_dev);
		return ret;
	}

	return 0;
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6781_drv);
	platform_device_unregister(clk_fmeter_dev);
}

module_init(clk_fmeter_init);
module_exit(clk_fmeter_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MT6781 Frequency Meter Driver");
