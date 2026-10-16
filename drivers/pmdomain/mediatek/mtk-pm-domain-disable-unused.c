// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

static int scpsys_disable_unused_probe(struct platform_device *pdev)
{
	pm_runtime_enable(&pdev->dev);
	return 0;
}

static const struct of_device_id scpsys_disable_unused_id_table[] = {
	{ .compatible = "mediatek,scpsys-disable-unused", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, scpsys_disable_unused_id_table);

static struct platform_driver scpsys_disable_unused_driver = {
	.probe = scpsys_disable_unused_probe,
	.driver = {
		.name = "scpsys_disable_unused",
		.of_match_table = scpsys_disable_unused_id_table,
	},
};

module_platform_driver(scpsys_disable_unused_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Power Domain Disable Unused Driver");
