// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

static int disable_unused_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int clk_count, i;
	struct clk *clk, *parent;

	if (!node)
		return -EINVAL;

	clk_count = of_count_phandle_with_args(node, "clocks", "#clock-cells");
	if (clk_count <= 0)
		return 0;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_resume_and_get(&pdev->dev);

	for (i = 0; i < clk_count; i++) {
		clk = of_clk_get(node, i);
		if (IS_ERR(clk)) {
			pr_notice("get clk [%d] fail, ret=%ld, clk_con=%d\n",
				  i, PTR_ERR(clk), clk_count);
			continue;
		}

		if (!__clk_get_hw(clk)) {
			pr_notice("clk [%d] is not ready\n", i);
			clk_put(clk);
			continue;
		}

		parent = clk_get_parent(clk);
		if (parent) {
			if (clk_prepare_enable(parent))
				pr_info("parent of clk %s clk_prepare_enable failed\n",
					__clk_get_name(clk));
		}

		if (clk_prepare_enable(clk))
			pr_info("clk %s clk_prepare_enable failed\n",
				__clk_get_name(clk));

		if (__clk_is_enabled(clk) && clk_hw_is_prepared(__clk_get_hw(clk)))
			clk_disable_unprepare(clk);

		if (parent)
			clk_disable_unprepare(parent);

		pr_notice("disable_unused clk [%d]: %s done\n",
			  i, __clk_get_name(clk));
		clk_put(clk);
	}

	pm_runtime_idle(&pdev->dev);
	return 0;
}

static const struct of_device_id disable_unused_id_table[] = {
	{ .compatible = "mediatek,clk-disable-unused", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, disable_unused_id_table);

static struct platform_driver clk_disable_unused_driver = {
	.probe = disable_unused_probe,
	.driver = {
		.name = "clk_disable_unused",
		.of_match_table = disable_unused_id_table,
	},
};

module_platform_driver(clk_disable_unused_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Clock Disable Unused Driver");
