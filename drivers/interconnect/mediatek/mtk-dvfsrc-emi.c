// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2024 MediaTek Inc.
 * Author: Henry Chen <henryc.chen@mediatek.com>
 */

#include <dt-bindings/interconnect/mtk,emi.h>
#include <linux/device.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>

#define MAX_LINKS	6

struct mtk_icc_node {
	unsigned char *name;
	bool ep;
	u16 id;
	u16 links[MAX_LINKS];
	u16 num_links;
	u16 buswidth;
	u64 sum_avg;
	u64 sum_peak;
};

struct mtk_icc_desc {
	struct mtk_icc_node **nodes;
	size_t num_nodes;
};

#define DEFINE_MNODE(_name, _id, _buswidth, _ep, _numlinks, ...)	\
		static struct mtk_icc_node _name = {			\
		.name = #_name,						\
		.id = _id,						\
		.buswidth = _buswidth,					\
		.ep = _ep,						\
		.num_links = ARRAY_SIZE(((int[]){ __VA_ARGS__ })),	\
		.links = { __VA_ARGS__ },				\
}

DEFINE_MNODE(ddr_emi, 0, 1024, 1, 0, 0);
DEFINE_MNODE(mcusys, 1, 256, 0, 1, 0);
DEFINE_MNODE(gpu, 2, 256, 0, 1, 0);
DEFINE_MNODE(mmsys, 3, 256, 0, 1, 0);
DEFINE_MNODE(mm_vpu, 4, 128, 0, 1, 3);
DEFINE_MNODE(mm_disp, 5, 128, 0, 1, 3);
DEFINE_MNODE(mm_vdec, 6, 128, 0, 1, 3);
DEFINE_MNODE(mm_venc, 7, 128, 0, 1, 3);
DEFINE_MNODE(mm_cam, 8, 128, 0, 1, 3);
DEFINE_MNODE(mm_img, 9, 128, 0, 1, 3);
DEFINE_MNODE(mm_mdp, 10, 128, 0, 1, 3);
DEFINE_MNODE(vpusys, 11, 256, 0, 1, 0);
DEFINE_MNODE(vpu_port_0, 12, 128, 0, 1, 11);
DEFINE_MNODE(vpu_port_1, 13, 128, 0, 1, 11);
DEFINE_MNODE(netsys, 14, 256, 0, 1, 0);
DEFINE_MNODE(bt, 15, 256, 0, 1, 0);
DEFINE_MNODE(hrt_adsp, 16, 256, 0, 1, 0);
DEFINE_MNODE(hrt_ddr_emi, 17, 1024, 1, 0, 0);
DEFINE_MNODE(hrt_mmsys, 18, 256, 0, 1, 17);
DEFINE_MNODE(hrt_mm_disp, 19, 128, 0, 1, 18);
DEFINE_MNODE(hrt_mm_vdec, 20, 128, 0, 1, 18);
DEFINE_MNODE(hrt_mm_venc, 21, 128, 0, 1, 18);
DEFINE_MNODE(hrt_mm_cam, 22, 128, 0, 1, 18);
DEFINE_MNODE(hrt_mm_img, 23, 128, 0, 1, 18);
DEFINE_MNODE(hrt_mm_mdp, 24, 128, 0, 1, 18);
DEFINE_MNODE(mcu_port_0, 25, 128, 0, 1, 1);
DEFINE_MNODE(mcu_port_1, 26, 128, 0, 1, 1);
DEFINE_MNODE(mcu_port_2, 27, 128, 0, 1, 1);
DEFINE_MNODE(mcu_port_3, 28, 128, 0, 1, 1);
DEFINE_MNODE(mcu_port_4, 29, 128, 0, 1, 1);

static struct mtk_icc_node *mt6781_icc_nodes[] = {
	&ddr_emi,
	&mcusys,
	&gpu,
	&mmsys,
	&mm_vpu,
	&mm_disp,
	&mm_vdec,
	&mm_venc,
	&mm_cam,
	&mm_img,
	&mm_mdp,
	&vpusys,
	&vpu_port_0,
	&vpu_port_1,
	&netsys,
	&bt,
	&hrt_adsp,
	&hrt_ddr_emi,
	&hrt_mmsys,
	&hrt_mm_disp,
	&hrt_mm_vdec,
	&hrt_mm_venc,
	&hrt_mm_cam,
	&hrt_mm_img,
	&hrt_mm_mdp,
	&mcu_port_0,
	&mcu_port_1,
	&mcu_port_2,
	&mcu_port_3,
	&mcu_port_4,
};

static struct mtk_icc_desc mt6781_icc = {
	.nodes = mt6781_icc_nodes,
	.num_nodes = ARRAY_SIZE(mt6781_icc_nodes),
};

static int emi_icc_aggregate(struct icc_node *node, u32 tag, u32 avg_bw,
			      u32 peak_bw, u32 *agg_avg, u32 *agg_peak)
{
	struct mtk_icc_node *in = node->data;

	*agg_avg += avg_bw;
	*agg_peak += peak_bw;

	in->sum_avg = *agg_avg;
	in->sum_peak = *agg_peak;

	return 0;
}

static int emi_icc_set(struct icc_node *src, struct icc_node *dst)
{
	struct mtk_icc_node *node = dst->data;

	if (node->ep) {
		pr_debug("sum_avg (%llu), max_peak (%llu)\n",
			 node->sum_avg, node->sum_peak);
		mtk_dvfsrc_send_request(src->provider->dev->parent,
					MTK_DVFSRC_CMD_BW_REQUEST,
					node->sum_avg);
		mtk_dvfsrc_send_request(src->provider->dev->parent,
					MTK_DVFSRC_CMD_HRTBW_REQUEST,
					node->sum_peak);
	}

	return 0;
}

static int emi_icc_probe(struct platform_device *pdev)
{
	const struct mtk_icc_desc *desc;
	struct icc_node *node;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct mtk_icc_node **mnodes;
	size_t num_nodes, i, j;
	int ret;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	mnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	provider = devm_kzalloc(&pdev->dev, sizeof(*provider), GFP_KERNEL);
	if (!provider)
		return -ENOMEM;

	data = devm_kcalloc(&pdev->dev, num_nodes, sizeof(*node), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	icc_provider_init(provider);
	provider->dev = &pdev->dev;
	provider->set = emi_icc_set;
	provider->aggregate = emi_icc_aggregate;
	provider->xlate = of_icc_xlate_onecell;
	provider->data = data;

	for (i = 0; i < num_nodes; i++) {
		node = icc_node_create(mnodes[i]->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}

		node->name = mnodes[i]->name;
		node->data = mnodes[i];
		icc_node_add(node, provider);

		for (j = 0; j < mnodes[i]->num_links; j++)
			icc_link_create(node, mnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	ret = icc_provider_register(provider);
	if (ret) {
		dev_err(&pdev->dev, "error adding interconnect provider\n");
		goto err;
	}

	platform_set_drvdata(pdev, provider);
	return 0;

err:
	icc_nodes_remove(provider);
	return ret;
}

static int emi_icc_remove(struct platform_device *pdev)
{
	struct icc_provider *provider = platform_get_drvdata(pdev);

	icc_provider_deregister(provider);
	icc_nodes_remove(provider);
	return 0;
}

static const struct of_device_id emi_icc_of_match[] = {
	{ .compatible = "mediatek,mt6781-dvfsrc", .data = &mt6781_icc },
	{ .compatible = "mediatek,mt6768-dvfsrc", .data = &mt6781_icc },
	{ .compatible = "mediatek,mt6833-dvfsrc", .data = &mt6781_icc },
	{ .compatible = "mediatek,mt6853-dvfsrc", .data = &mt6781_icc },
	{ .compatible = "mediatek,mt6873-dvfsrc", .data = &mt6781_icc },
	{ .compatible = "mediatek,mt6877-dvfsrc", .data = &mt6781_icc },
	{ },
};
MODULE_DEVICE_TABLE(of, emi_icc_of_match);

static struct platform_driver emi_icc_driver = {
	.probe = emi_icc_probe,
	.remove = emi_icc_remove,
	.driver = {
		.name = "mediatek-emi-icc",
		.of_match_table = emi_icc_of_match,
		.sync_state = icc_sync_state,
	},
};
module_platform_driver(emi_icc_driver);

MODULE_AUTHOR("Henry Chen <henryc.chen@mediatek.com>");
MODULE_DESCRIPTION("MediaTek EMI Interconnect Driver");
MODULE_LICENSE("GPL v2");
