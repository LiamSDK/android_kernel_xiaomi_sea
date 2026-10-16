// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2024 MediaTek Inc.
 * Author: Ming-Fan Chen <ming-fan.chen@mediatek.com>
 *         Anthony Huang <anthony.huang@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <soc/mediatek/smi.h>

#include "mmqos-mtk.h"

#define SHIFT_ROUND(a, b)	((((a) - 1) >> (b)) + 1)
#define icc_to_MBps(x)		((x) / 1000)

static struct mmqos_hrt *mmqos_hrt;

/* HRT functions */
s32 mtk_mmqos_get_avail_hrt_bw(enum hrt_type type)
{
	u32 i, used_bw = 0;

	if (!mmqos_hrt)
		return 0;

	for (i = 0; i < HRT_TYPE_NUM; i++) {
		if (i != type)
			used_bw += mmqos_hrt->hrt_bw[i];
	}

	if (mmqos_hrt->cam_max_bw)
		used_bw = used_bw - mmqos_hrt->hrt_bw[HRT_CAM] + mmqos_hrt->cam_max_bw;

	return (mmqos_hrt->hrt_total_bw - used_bw);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_get_avail_hrt_bw);

s32 mtk_mmqos_register_bw_throttle_notifier(struct notifier_block *nb)
{
	if (!mmqos_hrt)
		return -ENODEV;
	return blocking_notifier_chain_register(&mmqos_hrt->hrt_bw_throttle_notifier, nb);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_register_bw_throttle_notifier);

s32 mtk_mmqos_unregister_bw_throttle_notifier(struct notifier_block *nb)
{
	if (!mmqos_hrt)
		return -ENODEV;
	return blocking_notifier_chain_unregister(&mmqos_hrt->hrt_bw_throttle_notifier, nb);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_unregister_bw_throttle_notifier);

void mtk_mmqos_wait_throttle_done(void)
{
	if (mmqos_hrt && atomic_read(&mmqos_hrt->lock_count) > 0) {
		wait_event_timeout(mmqos_hrt->hrt_wait,
				   atomic_read(&mmqos_hrt->lock_count) == 0,
				   msecs_to_jiffies(200));
	}
}
EXPORT_SYMBOL_GPL(mtk_mmqos_wait_throttle_done);

s32 mtk_mmqos_set_hrt_bw(enum hrt_type type, u32 bw)
{
	if (type >= HRT_TYPE_NUM)
		return -EINVAL;

	if (!mmqos_hrt)
		return -ENODEV;

	mmqos_hrt->hrt_bw[type] = bw;
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_set_hrt_bw);

u32 mtk_mmqos_get_cam_hrt(void)
{
	if (!mmqos_hrt)
		return 0;
	return mmqos_hrt->cam_max_bw;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_get_cam_hrt);

u32 mtk_mmqos_get_hrt_ratio(void)
{
	return 100;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_get_hrt_ratio);

void mtk_mmqos_hrt_scen(u32 scen)
{
}
EXPORT_SYMBOL_GPL(mtk_mmqos_hrt_scen);

void mtk_mmqos_init_hrt(struct mmqos_hrt *hrt)
{
	mmqos_hrt = hrt;
	BLOCKING_INIT_NOTIFIER_HEAD(&hrt->hrt_bw_throttle_notifier);
	init_waitqueue_head(&hrt->hrt_wait);
	atomic_set(&hrt->lock_count, 0);
	mutex_init(&hrt->blocking_lock);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_init_hrt);

static ssize_t hrt_total_bw_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", mmqos_hrt ? mmqos_hrt->hrt_total_bw : 0);
}

static ssize_t hrt_total_bw_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	u32 val;
	if (kstrtou32(buf, 0, &val))
		return -EINVAL;
	if (mmqos_hrt)
		mmqos_hrt->hrt_total_bw = val;
	return count;
}
static DEVICE_ATTR_RW(hrt_total_bw);

static ssize_t cam_max_bw_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", mmqos_hrt ? mmqos_hrt->cam_max_bw : 0);
}

static ssize_t cam_max_bw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u32 val;
	if (kstrtou32(buf, 0, &val))
		return -EINVAL;
	if (mmqos_hrt)
		mmqos_hrt->cam_max_bw = val;
	return count;
}
static DEVICE_ATTR_RW(cam_max_bw);

static ssize_t blocking_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", mmqos_hrt ? mmqos_hrt->blocking : 0);
}

static ssize_t blocking_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	bool val;
	if (kstrtobool(buf, &val))
		return -EINVAL;
	if (mmqos_hrt)
		mmqos_hrt->blocking = val;
	return count;
}
static DEVICE_ATTR_RW(blocking);

static struct attribute *mmqos_hrt_attrs[] = {
	&dev_attr_hrt_total_bw.attr,
	&dev_attr_cam_max_bw.attr,
	&dev_attr_blocking.attr,
	NULL,
};

static const struct attribute_group mmqos_hrt_group = {
	.name = "hrt",
	.attrs = mmqos_hrt_attrs,
};

int mtk_mmqos_register_hrt_sysfs(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &mmqos_hrt_group);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_register_hrt_sysfs);

void mtk_mmqos_unregister_hrt_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &mmqos_hrt_group);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_unregister_hrt_sysfs);

/* Provider and SMI update */
static void mmqos_update_comm_bw(struct device *dev,
				 u32 comm_port, u32 freq, u64 mix_bw, u64 bw_peak, bool qos_bound)
{
	u32 comm_bw = 0;
	u32 value;

	if (!freq || !dev)
		return;

	if (mix_bw)
		comm_bw = (mix_bw << 8) / freq;

	if (comm_bw)
		value = ((comm_bw > 0xfff) ? 0xfff : comm_bw) |
			((bw_peak > 0 || !qos_bound) ? 0x1000 : 0x3000);
	else
		value = 0x1200;

	mtk_smi_common_bw_set(dev, comm_port, value);
}

static void set_comm_icc_bw_handler(struct work_struct *work)
{
	struct common_node *comm_node = container_of(work, struct common_node, work);
	struct mtk_mmqos *mmqos = container_of(comm_node->base->icc_node->provider,
					       struct mtk_mmqos, prov);
	struct common_port_node *comm_port_node;
	u32 avg_bw = 0, peak_bw = 0;

	list_for_each_entry(comm_port_node, &mmqos->comm_port_list, list) {
		mutex_lock(&comm_port_node->bw_lock);
		avg_bw += comm_port_node->latest_avg_bw;
		peak_bw += (comm_port_node->latest_peak_bw & ~(MTK_MMQOS_MAX_BW));
		mutex_unlock(&comm_port_node->bw_lock);
	}
	icc_set_bw(comm_node->icc_path, avg_bw, peak_bw);
}

static int mtk_mmqos_set(struct icc_node *src, struct icc_node *dst)
{
	struct larb_node *larb_node;
	struct larb_port_node *larb_port_node;
	struct common_port_node *comm_port_node;
	struct common_node *comm_node;
	struct mtk_mmqos *mmqos = container_of(dst->provider, struct mtk_mmqos, prov);
	u32 value = 1;

	switch (dst->id >> 16) {
	case MTK_MMQOS_NODE_COMMON:
		comm_node = (struct common_node *)dst->data;
		if (comm_node)
			queue_work(mmqos->wq, &comm_node->work);
		break;
	case MTK_MMQOS_NODE_COMMON_PORT:
		comm_port_node = (struct common_port_node *)dst->data;
		if (!comm_port_node)
			break;
		mutex_lock(&comm_port_node->bw_lock);
		comm_port_node->latest_mix_bw = comm_port_node->base->mix_bw;
		comm_port_node->latest_peak_bw = dst->peak_bw;
		comm_port_node->latest_avg_bw = dst->avg_bw;
		mmqos_update_comm_bw(comm_port_node->larb_dev,
				     dst->id & 0xff, comm_port_node->common ? comm_port_node->common->freq : 0,
				     icc_to_MBps(comm_port_node->latest_mix_bw),
				     icc_to_MBps(comm_port_node->latest_peak_bw),
				     mmqos->qos_bound);
		mutex_unlock(&comm_port_node->bw_lock);
		break;
	case MTK_MMQOS_NODE_LARB:
		larb_port_node = (struct larb_port_node *)src->data;
		larb_node = (struct larb_node *)dst->data;
		if (!larb_port_node || !larb_node)
			break;
		if (larb_port_node->base->mix_bw)
			value = SHIFT_ROUND(icc_to_MBps(larb_port_node->base->mix_bw),
					    larb_port_node->bw_ratio);
		if (value > mmqos->max_ratio)
			value = mmqos->max_ratio;
		mtk_smi_larb_bw_set(larb_node->larb_dev, src->id & 0xff, value);

		if ((dst->id & 0xff) == 1) {
			mtk_smi_larb_bw_set(larb_node->larb_dev, 9, 8);
			mtk_smi_larb_bw_set(larb_node->larb_dev, 11, 8);
		}
		break;
	default:
		break;
	}
	return 0;
}

static int mtk_mmqos_aggregate(struct icc_node *node,
			       u32 avg_bw, u32 peak_bw, u32 *agg_avg, u32 *agg_peak)
{
	struct mmqos_base_node *base_node = NULL;
	u32 mix_bw = peak_bw;

	switch (node->id >> 16) {
	case MTK_MMQOS_NODE_LARB_PORT:
		if (node->data)
			base_node = ((struct larb_node *)node->data)->base;
		if (peak_bw)
			mix_bw = SHIFT_ROUND(peak_bw * 3, 1);
		break;
	case MTK_MMQOS_NODE_COMMON_PORT:
		if (node->data)
			base_node = ((struct common_port_node *)node->data)->base;
		break;
	default:
		return 0;
	}

	if (base_node) {
		if (*agg_avg == 0 && *agg_peak == 0)
			base_node->mix_bw = 0;
		base_node->mix_bw += peak_bw ? mix_bw : avg_bw;
	}

	*agg_avg += avg_bw;
	if (peak_bw == MTK_MMQOS_MAX_BW)
		*agg_peak |= MTK_MMQOS_MAX_BW;
	else
		*agg_peak += peak_bw;
	return 0;
}

static struct icc_node *mtk_mmqos_xlate(struct of_phandle_args *spec, void *data)
{
	struct icc_onecell_data *icc_data = (struct icc_onecell_data *)data;
	s32 i;

	for (i = 0; i < icc_data->num_nodes; i++) {
		if (icc_data->nodes[i]->id == spec->args[0])
			return icc_data->nodes[i];
	}

	pr_notice("%s: invalid index %u\n", __func__, spec->args[0]);
	return ERR_PTR(-EINVAL);
}

int mtk_mmqos_probe(struct platform_device *pdev)
{
	struct mtk_mmqos *mmqos;
	struct of_phandle_iterator it;
	struct icc_onecell_data *data;
	struct icc_node *node;
	struct mmqos_base_node *base_node;
	struct common_node *comm_node;
	struct common_port_node *comm_port_node;
	struct larb_node *larb_node;
	struct larb_port_node *larb_port_node;
	struct mtk_smi_iommu smi_imu;
	int i, id, num_larbs = 0, ret;
	const struct mtk_mmqos_desc *mmqos_desc;
	const struct mtk_node_desc *node_desc;
	struct device *larb_dev;
	struct mmqos_hrt *hrt;

	mmqos = devm_kzalloc(&pdev->dev, sizeof(*mmqos), GFP_KERNEL);
	if (!mmqos)
		return -ENOMEM;
	mmqos->dev = &pdev->dev;

	memset(&smi_imu, 0, sizeof(smi_imu));
	of_for_each_phandle(&it, ret, pdev->dev.of_node, "mediatek,larbs", NULL, 0) {
		struct device_node *np;
		struct platform_device *larb_pdev;

		np = of_node_get(it.node);
		if (!of_device_is_available(np)) {
			of_node_put(np);
			continue;
		}

		larb_pdev = of_find_device_by_node(np);
		if (!larb_pdev) {
			larb_pdev = of_platform_device_create(np, NULL, platform_bus_type.dev_root);
			if (!larb_pdev || !larb_pdev->dev.driver) {
				of_node_put(np);
				return -EPROBE_DEFER;
			}
		}

		if (of_property_read_u32(np, "mediatek,larb-id", &id))
			id = num_larbs;
		if (id < MTK_LARB_NR_MAX)
			smi_imu.larb_imu[id].dev = &larb_pdev->dev;
		num_larbs++;
		of_node_put(np);
	}

	INIT_LIST_HEAD(&mmqos->comm_list);
	INIT_LIST_HEAD(&mmqos->comm_port_list);

	icc_provider_init(&mmqos->prov);
	mmqos->prov.set = mtk_mmqos_set;
	mmqos->prov.aggregate = mtk_mmqos_aggregate;
	mmqos->prov.xlate = mtk_mmqos_xlate;
	mmqos->prov.dev = &pdev->dev;

	ret = icc_provider_register(&mmqos->prov);
	if (ret) {
		dev_err(&pdev->dev, "icc_provider_register failed: %d\n", ret);
		return ret;
	}

	mmqos_desc = of_device_get_match_data(&pdev->dev);
	if (!mmqos_desc) {
		ret = -EINVAL;
		goto err_prov;
	}

	hrt = devm_kzalloc(&pdev->dev, sizeof(*hrt), GFP_KERNEL);
	if (!hrt) {
		ret = -ENOMEM;
		goto err_prov;
	}
	memcpy(hrt, &mmqos_desc->hrt, sizeof(mmqos_desc->hrt));
	mtk_mmqos_init_hrt(hrt);

	data = devm_kzalloc(&pdev->dev, struct_size(data, nodes, mmqos_desc->num_nodes), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err_prov;
	}

	for (i = 0; i < mmqos_desc->num_nodes; i++) {
		node_desc = &mmqos_desc->nodes[i];
		node = icc_node_create(node_desc->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err_nodes;
		}
		icc_node_add(node, &mmqos->prov);

		if (node_desc->link != MMQOS_NO_LINK) {
			ret = icc_link_create(node, node_desc->link);
			if (ret)
				goto err_nodes;
		}
		node->name = node_desc->name;

		base_node = devm_kzalloc(&pdev->dev, sizeof(*base_node), GFP_KERNEL);
		if (!base_node) {
			ret = -ENOMEM;
			goto err_nodes;
		}
		base_node->icc_node = node;

		switch (node->id >> 16) {
		case MTK_MMQOS_NODE_COMMON:
			comm_node = devm_kzalloc(&pdev->dev, sizeof(*comm_node), GFP_KERNEL);
			if (!comm_node) {
				ret = -ENOMEM;
				goto err_nodes;
			}
			INIT_WORK(&comm_node->work, set_comm_icc_bw_handler);
			if (mmqos_desc->comm_muxes && mmqos_desc->comm_muxes[node->id & 0xff]) {
				comm_node->clk = devm_clk_get(&pdev->dev,
							      mmqos_desc->comm_muxes[node->id & 0xff]);
				if (!IS_ERR(comm_node->clk))
					comm_node->freq = clk_get_rate(comm_node->clk) / 1000000;
			}
			INIT_LIST_HEAD(&comm_node->list);
			list_add_tail(&comm_node->list, &mmqos->comm_list);
			if (mmqos_desc->comm_icc_path_names && mmqos_desc->comm_icc_path_names[node->id & 0xff])
				comm_node->icc_path = of_icc_get(&pdev->dev,
								 mmqos_desc->comm_icc_path_names[node->id & 0xff]);
			comm_node->base = base_node;
			node->data = (void *)comm_node;
			break;
		case MTK_MMQOS_NODE_COMMON_PORT:
			comm_port_node = devm_kzalloc(&pdev->dev, sizeof(*comm_port_node), GFP_KERNEL);
			if (!comm_port_node) {
				ret = -ENOMEM;
				goto err_nodes;
			}
			mutex_init(&comm_port_node->bw_lock);
			if (node->num_links > 0 && node->links[0])
				comm_port_node->common = node->links[0]->data;
			INIT_LIST_HEAD(&comm_port_node->list);
			list_add_tail(&comm_port_node->list, &mmqos->comm_port_list);
			comm_port_node->base = base_node;
			node->data = (void *)comm_port_node;
			break;
		case MTK_MMQOS_NODE_LARB:
			larb_node = devm_kzalloc(&pdev->dev, sizeof(*larb_node), GFP_KERNEL);
			if (!larb_node) {
				ret = -ENOMEM;
				goto err_nodes;
			}
			if (node->num_links > 0 && node->links[0])
				comm_port_node = node->links[0]->data;
			else
				comm_port_node = NULL;

			id = node->id & (MTK_LARB_NR_MAX - 1);
			larb_dev = smi_imu.larb_imu[id].dev;
			if (larb_dev) {
				if (comm_port_node)
					comm_port_node->larb_dev = larb_dev;
				larb_node->larb_dev = larb_dev;
			}
			larb_node->base = base_node;
			node->data = (void *)larb_node;
			break;
		case MTK_MMQOS_NODE_LARB_PORT:
			larb_port_node = devm_kzalloc(&pdev->dev, sizeof(*larb_port_node), GFP_KERNEL);
			if (!larb_port_node) {
				ret = -ENOMEM;
				goto err_nodes;
			}
			larb_port_node->bw_ratio = node_desc->bw_ratio;
			larb_port_node->base = base_node;
			node->data = (void *)larb_port_node;
			break;
		default:
			break;
		}
		data->nodes[i] = node;
	}

	data->num_nodes = mmqos_desc->num_nodes;
	mmqos->prov.data = data;
	mmqos->max_ratio = mmqos_desc->max_ratio;

	mmqos->wq = create_singlethread_workqueue("mmqos_wq");
	if (!mmqos->wq) {
		ret = -ENOMEM;
		goto err_nodes;
	}

	mtk_mmqos_register_hrt_sysfs(&pdev->dev);
	platform_set_drvdata(pdev, mmqos);

	return 0;

err_nodes:
	icc_nodes_remove(&mmqos->prov);
err_prov:
	icc_provider_deregister(&mmqos->prov);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_probe);

int mtk_mmqos_remove(struct platform_device *pdev)
{
	struct mtk_mmqos *mmqos = platform_get_drvdata(pdev);

	mtk_mmqos_unregister_hrt_sysfs(&pdev->dev);
	if (mmqos->wq)
		destroy_workqueue(mmqos->wq);
	icc_nodes_remove(&mmqos->prov);
	icc_provider_deregister(&mmqos->prov);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_remove);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MMQoS Core Driver");
