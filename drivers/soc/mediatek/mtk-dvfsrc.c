// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2024 MediaTek Inc.
 * MTK DVFSRC driver
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>

#define MTK_SIP_GET_SPM_INFO		0x3
#define MTK_SIP_VCOREFS_CONTROL		0x82000226

#define DVFSRC_GET_TARGET_LEVEL(x)	(((x) >> 0) & 0x0000ffff)
#define DVFSRC_GET_CURRENT_LEVEL(x)	(((x) >> 16) & 0x0000ffff)
#define kBps_to_MBps(x)			(div_u64(x, 1000))
#define MBps_to_kBps(x)			((x) * 1000)

#define POLL_TIMEOUT			1000
#define STARTUP_TIME			1

enum dvfsrc_regs {
	DVFSRC_BASIC_CONTROL,
	DVFSRC_SW_REQ,
	DVFSRC_EXT_SW_REQ,
	DVFSRC_TARGET_LEVEL,
	DVFSRC_LEVEL,
	DVFSRC_SW_BW,
	DVFSRC_SW_PEAK_BW,
	DVFSRC_SW_HRTBW,
	DVFSRC_SW_EXT_BW,
	DVFSRC_SEC_SW_REQ,
	DVFSRC_TARGET_FORCE,
	DVFSRC_MD_BW,
	DVFSRC_INT,
	DVFSRC_INT_EN,
	DVFSRC_VCORE_REQUEST,
	DVFSRC_VCORE_FORCE,
	DVFSRC_REGS_MAX,
};

static const int mt6781_regs[] = {
	[DVFSRC_BASIC_CONTROL]	= 0x0,
	[DVFSRC_SW_REQ]		= 0xc,
	[DVFSRC_EXT_SW_REQ]	= 0x0,
	[DVFSRC_TARGET_LEVEL]	= 0xd44,
	[DVFSRC_LEVEL]		= 0xd48,
	[DVFSRC_SW_BW]		= 0x26c,
	[DVFSRC_SW_PEAK_BW]	= 0x278,
	[DVFSRC_SW_HRTBW]	= 0x290,
	[DVFSRC_SW_EXT_BW]	= 0x6c,
	[DVFSRC_SEC_SW_REQ]	= 0x0,
	[DVFSRC_TARGET_FORCE]	= 0xd70,
	[DVFSRC_MD_BW]		= 0x0,
	[DVFSRC_INT]		= 0x0,
	[DVFSRC_INT_EN]		= 0x0,
	[DVFSRC_VCORE_REQUEST]	= 0xc4,
	[DVFSRC_VCORE_FORCE]	= 0xc8,
};

struct dvfsrc_opp {
	u32 vcore_opp;
	u32 dram_opp;
};

struct dvfsrc_opp_desc {
	const struct dvfsrc_opp *opps;
	u32 size;
};

struct dvfsrc_soc_data {
	const int *regs;
	u32 num_opp;
	const struct dvfsrc_opp_desc *opp_desc;
	int (*get_target_level)(struct mtk_dvfsrc *dvfsrc);
	int (*get_current_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vcore_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vcp_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_dram_level)(struct mtk_dvfsrc *dvfsrc);
	void (*set_dram_bw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_dram_peak_bw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_dram_hrtbw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_dram_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_vscp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_dram_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*set_force_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
};

struct mtk_dvfsrc {
	struct device *dev;
	const struct dvfsrc_soc_data *dvd;
	void __iomem *regs;
	struct mutex lock;
	struct mutex sw_req_lock;
	int dram_type;
	bool opp_forced;
};

static int dram_type;
static DEFINE_SPINLOCK(force_req_lock);
static BLOCKING_NOTIFIER_HEAD(dvfsrc_debug_notifier);

static inline u32 dvfsrc_read(struct mtk_dvfsrc *dvfsrc, enum dvfsrc_regs reg)
{
	return readl(dvfsrc->regs + dvfsrc->dvd->regs[reg]);
}

static inline void dvfsrc_write(struct mtk_dvfsrc *dvfsrc, enum dvfsrc_regs reg, u32 val)
{
	writel(val, dvfsrc->regs + dvfsrc->dvd->regs[reg]);
}

static inline void dvfsrc_rmw(struct mtk_dvfsrc *dvfsrc, enum dvfsrc_regs reg,
			     u32 val, u32 mask, u32 shift)
{
	u32 r = dvfsrc_read(dvfsrc, reg);

	r &= ~(mask << shift);
	r |= (val & mask) << shift;
	dvfsrc_write(dvfsrc, reg, r);
}

/* OPP Tables */
static const struct dvfsrc_opp dvfsrc_opp_mt6781[] = {
	{0, 0}, {0, 1}, {0, 2}, {0, 3},
	{1, 1}, {1, 2}, {1, 3}, {1, 4},
	{2, 1}, {2, 2}, {2, 3}, {2, 4},
	{2, 5},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6781_desc = {
	.opps = dvfsrc_opp_mt6781,
	.size = ARRAY_SIZE(dvfsrc_opp_mt6781),
};

static int mt6873_get_target_level(struct mtk_dvfsrc *dvfsrc)
{
	return dvfsrc_read(dvfsrc, DVFSRC_TARGET_LEVEL);
}

static int mt6873_get_current_level(struct mtk_dvfsrc *dvfsrc)
{
	return dvfsrc_read(dvfsrc, DVFSRC_LEVEL);
}

static u32 mt6873_get_vcore_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) >> 2) & 0x3;
}

static u32 mt6873_get_vcp_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST) >> 30) & 0x3;
}

static u32 mt6873_get_dram_level(struct mtk_dvfsrc *dvfsrc)
{
	return dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) & 0x3;
}

static void mt6873_set_dram_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kBps_to_MBps(bw), 100);
	if (bw > 0x3ff)
		bw = 0x3ff;
	dvfsrc_write(dvfsrc, DVFSRC_SW_BW, bw);
}

static void mt6873_set_dram_peak_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kBps_to_MBps(bw), 100);
	if (bw > 0x3ff)
		bw = 0x3ff;
	dvfsrc_write(dvfsrc, DVFSRC_SW_PEAK_BW, bw);
}

static void mt6873_set_dram_hrtbw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kBps_to_MBps(bw), 100);
	if (bw > 0x3ff)
		bw = 0x3ff;
	dvfsrc_write(dvfsrc, DVFSRC_SW_HRTBW, bw);
}

static void mt6873_set_dram_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	mutex_lock(&dvfsrc->sw_req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x7, 0);
	mutex_unlock(&dvfsrc->sw_req_lock);
}

static void mt6873_set_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *opp;

	if (level >= dvfsrc->dvd->opp_desc->size)
		return;

	opp = &dvfsrc->dvd->opp_desc->opps[level];
	mt6873_set_dram_level(dvfsrc, opp->dram_opp);
}

static void mt6873_set_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	mutex_lock(&dvfsrc->sw_req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x3, 4);
	mutex_unlock(&dvfsrc->sw_req_lock);
}

static void mt6873_set_vscp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dvfsrc_rmw(dvfsrc, DVFSRC_VCORE_REQUEST, level, 0x3, 30);
}

static int mt6873_wait_for_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	u32 val;
	return readl_poll_timeout_atomic(dvfsrc->regs + dvfsrc->dvd->regs[DVFSRC_LEVEL],
					val, (val & 0xffff) == level,
					STARTUP_TIME, POLL_TIMEOUT);
}

static int dvfsrc_wait_for_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	u32 val;
	return readl_poll_timeout_atomic(dvfsrc->regs + dvfsrc->dvd->regs[DVFSRC_LEVEL],
					val, ((val >> 16) & 0x3) == level,
					STARTUP_TIME, POLL_TIMEOUT);
}

static int dvfsrc_wait_for_dram_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	u32 val;
	return readl_poll_timeout_atomic(dvfsrc->regs + dvfsrc->dvd->regs[DVFSRC_LEVEL],
					val, ((val >> 20) & 0x7) == level,
					STARTUP_TIME, POLL_TIMEOUT);
}

static int mt6873_set_force_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	unsigned long flags;
	int val, ret = 0;

	if (level >= dvfsrc->dvd->num_opp) {
		dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 0, 0x1, 15);
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
		dvfsrc->opp_forced = false;
		return 0;
	}

	dvfsrc->opp_forced = true;
	spin_lock_irqsave(&force_req_lock, flags);
	level = dvfsrc->dvd->num_opp - 1 - level;
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 1 << level);
	dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 1, 0x1, 15);
	ret = readl_poll_timeout_atomic(dvfsrc->regs + dvfsrc->dvd->regs[DVFSRC_LEVEL],
					val, DVFSRC_GET_CURRENT_LEVEL(val) == (1 << level),
					STARTUP_TIME, POLL_TIMEOUT);
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
	spin_unlock_irqrestore(&force_req_lock, flags);

	return ret;
}

static const struct dvfsrc_soc_data mt6781_data = {
	.regs = mt6781_regs,
	.num_opp = ARRAY_SIZE(dvfsrc_opp_mt6781),
	.opp_desc = &dvfsrc_opp_mt6781_desc,
	.get_target_level = mt6873_get_target_level,
	.get_current_level = mt6873_get_current_level,
	.get_vcore_level = mt6873_get_vcore_level,
	.get_vcp_level = mt6873_get_vcp_level,
	.get_dram_level = mt6873_get_dram_level,
	.set_dram_bw = mt6873_set_dram_bw,
	.set_dram_peak_bw = mt6873_set_dram_peak_bw,
	.set_dram_hrtbw = mt6873_set_dram_hrtbw,
	.set_dram_level = mt6873_set_dram_level,
	.set_opp_level = mt6873_set_opp_level,
	.set_vcore_level = mt6873_set_vcore_level,
	.set_vscp_level = mt6873_set_vscp_level,
	.wait_for_opp_level = mt6873_wait_for_opp_level,
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level,
	.wait_for_dram_level = dvfsrc_wait_for_dram_level,
	.set_force_opp_level = mt6873_set_force_opp_level,
};

/* Exported APIs */
u32 dvfsrc_get_required_opp_peak_bw(struct device_node *np, int index)
{
	struct device_node *opp_np;
	u32 peak_bw = 0;
	int ret;

	opp_np = of_parse_phandle(np, "required-opps", index);
	if (!opp_np) {
		pr_notice("%s: Unable to parse required-opps: %pOF, index: %d\n",
			  "dvfsrc_parse_required_opp", np, index);
		return 0;
	}

	ret = of_property_read_u32_index(opp_np, "opp-peak-KBps", dram_type, &peak_bw);
	if (ret)
		pr_info("%s: get fail\n", "dvfsrc_get_required_opp_peak_bw");

	of_node_put(opp_np);
	return peak_bw;
}
EXPORT_SYMBOL_GPL(dvfsrc_get_required_opp_peak_bw);

u32 dvfsrc_get_required_opp_emi_bw(struct device_node *np, int index)
{
	struct device_node *opp_np;
	u32 emi_bw = 0;
	int ret;

	opp_np = of_parse_phandle(np, "required-emi-opps", index);
	if (!opp_np) {
		pr_notice("%s: Unable to parse required-emi-opps: %pOF, index: %d\n",
			  "dvfsrc_parse_required_emi_opp", np, index);
		return 0;
	}

	ret = of_property_read_u32_index(opp_np, "opp-bw-KBps", dram_type, &emi_bw);
	if (ret)
		pr_info("%s: get fail\n", "dvfsrc_get_required_opp_emi_bw");

	of_node_put(opp_np);
	return emi_bw;
}
EXPORT_SYMBOL_GPL(dvfsrc_get_required_opp_emi_bw);

void dvfsrc_set_power_model_ddr_request(struct device *dev, u32 val)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	u32 reg;

	if (!dvfsrc)
		return;

	reg = readl(dvfsrc->regs + 0x618);
	reg = (reg & ~0x1f) | (val & 0x1f);
	writel(reg, dvfsrc->regs + 0x618);
}
EXPORT_SYMBOL_GPL(dvfsrc_set_power_model_ddr_request);

int register_dvfsrc_debug_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&dvfsrc_debug_notifier, nb);
}
EXPORT_SYMBOL_GPL(register_dvfsrc_debug_notifier);

int unregister_dvfsrc_debug_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&dvfsrc_debug_notifier, nb);
}
EXPORT_SYMBOL_GPL(unregister_dvfsrc_debug_notifier);

void mtk_dvfsrc_send_request(const struct device *dev, u32 cmd, u64 data)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (!dvfsrc || !dvfsrc->dvd)
		return;

	if (cmd == MTK_DVFSRC_CMD_FORCE_OPP_REQUEST) {
		if (dvfsrc->dvd->set_force_opp_level)
			dvfsrc->dvd->set_force_opp_level(dvfsrc, data);
		return;
	}

	switch (cmd) {
	case MTK_DVFSRC_CMD_BW_REQUEST:
		if (dvfsrc->dvd->set_dram_bw)
			dvfsrc->dvd->set_dram_bw(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_OPP_REQUEST:
		if (dvfsrc->dvd->set_opp_level)
			dvfsrc->dvd->set_opp_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_DRAM_REQUEST:
		if (dvfsrc->dvd->set_dram_level)
			dvfsrc->dvd->set_dram_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_VCORE_REQUEST:
		if (dvfsrc->dvd->set_vcore_level)
			dvfsrc->dvd->set_vcore_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_HRTBW_REQUEST:
		if (dvfsrc->dvd->set_dram_hrtbw)
			dvfsrc->dvd->set_dram_hrtbw(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_VSCP_REQUEST:
		if (dvfsrc->dvd->set_vscp_level)
			dvfsrc->dvd->set_vscp_level(dvfsrc, data);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(mtk_dvfsrc_send_request);

int mtk_dvfsrc_query_info(const struct device *dev, u32 cmd, int *data)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (!dvfsrc || !dvfsrc->dvd || !data)
		return -EINVAL;

	switch (cmd) {
	case MTK_DVFSRC_CMD_VCORE_QUERY:
		if (dvfsrc->dvd->get_vcore_level)
			*data = dvfsrc->dvd->get_vcore_level(dvfsrc);
		break;
	case MTK_DVFSRC_CMD_VCP_QUERY:
		if (dvfsrc->dvd->get_vcp_level)
			*data = dvfsrc->dvd->get_vcp_level(dvfsrc);
		break;
	case MTK_DVFSRC_CMD_CURR_LEVEL_QUERY:
		if (dvfsrc->dvd->get_current_level)
			*data = dvfsrc->dvd->get_current_level(dvfsrc);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_dvfsrc_query_info);

static int mtk_dvfsrc_probe(struct platform_device *pdev)
{
	struct arm_smccc_res ares;
	struct resource *res;
	struct mtk_dvfsrc *dvfsrc;
	int ret;

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dvd = of_device_get_match_data(&pdev->dev);
	if (!dvfsrc->dvd)
		return -EINVAL;
	dvfsrc->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dvfsrc->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	mutex_init(&dvfsrc->lock);
	mutex_init(&dvfsrc->sw_req_lock);

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_GET_SPM_INFO,
		      0, 0, 0, 0, 0, 0, &ares);
	if (!ares.a0) {
		dvfsrc->dram_type = ares.a1;
		dram_type = ares.a1;
	} else {
		dvfsrc->dram_type = 0;
		dram_type = 0;
	}

	dev_info(dvfsrc->dev, "dram_type: %d\n", dvfsrc->dram_type);

	platform_set_drvdata(pdev, dvfsrc);

	ret = devm_of_platform_populate(&pdev->dev);
	if (ret)
		dev_warn(&pdev->dev, "Failed to populate child devices: %d\n", ret);

	return 0;
}

static int mtk_dvfsrc_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mtk_dvfsrc_of_match[] = {
	{ .compatible = "mediatek,mt6781-dvfsrc", .data = &mt6781_data },
	{ .compatible = "mediatek,mt6768-dvfsrc", .data = &mt6781_data },
	{ .compatible = "mediatek,mt6833-dvfsrc", .data = &mt6781_data },
	{ .compatible = "mediatek,mt6853-dvfsrc", .data = &mt6781_data },
	{ .compatible = "mediatek,mt6877-dvfsrc", .data = &mt6781_data },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_dvfsrc_of_match);

static struct platform_driver mtk_dvfsrc_driver = {
	.probe	= mtk_dvfsrc_probe,
	.remove	= mtk_dvfsrc_remove,
	.driver = {
		.name = "mtk-dvfsrc",
		.of_match_table = of_match_ptr(mtk_dvfsrc_of_match),
	},
};

module_platform_driver(mtk_dvfsrc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC driver");
