// SPDX-License-Identifier: GPL-2.0
/*
 * MT6370 Debug Driver
 *
 * Copyright (c) 2024 Richtek Technology Corp.
 * Author: cy_huang <cy_huang@richtek.com>
 */

#include <linux/device.h>
#include <linux/extdev_io_class.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

struct mt6370_dbg_info {
	struct regmap *regmap;
	struct extdev_io_desc extdev_desc;
	struct extdev_io_dev *extdev;
};

static int mt6370_dbg_io_read(void *drvdata, u16 reg, void *val, u16 size)
{
	return regmap_bulk_read((struct regmap *)drvdata, reg, val, size);
}

static int mt6370_dbg_io_write(void *drvdata, u16 reg, const void *val, u16 size)
{
	return regmap_bulk_write((struct regmap *)drvdata, reg, val, size);
}

static int mt6370_dbg_probe(struct platform_device *pdev)
{
	struct mt6370_dbg_info *info;
	struct regmap *regmap;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "failed to allocate regmap\n");
		return -ENODEV;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->regmap = regmap;
	info->extdev_desc.devname = devm_kasprintf(&pdev->dev, GFP_KERNEL,
						  "MT6370.%s", dev_name(&pdev->dev));
	if (!info->extdev_desc.devname)
		return -ENOMEM;

	info->extdev_desc.typestr = "I2C,MT6370";
	info->extdev_desc.io_drvdata = regmap;
	info->extdev_desc.io_read = mt6370_dbg_io_read;
	info->extdev_desc.io_write = mt6370_dbg_io_write;

	info->extdev = devm_extdev_io_device_register(&pdev->dev, &info->extdev_desc);
	if (IS_ERR(info->extdev)) {
		dev_err(&pdev->dev, "%s:failed to register extdev_io device\n", __func__);
		return PTR_ERR(info->extdev);
	}

	platform_set_drvdata(pdev, info);
	dev_info(&pdev->dev, "%s: successfully\n", __func__);
	return 0;
}

static int mt6370_dbg_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mt6370_dbg_of_id[] = {
	{ .compatible = "mediatek,mt6370-dbg", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6370_dbg_of_id);

static const struct platform_device_id mt6370_dbg_id[] = {
	{ "mt6370-dbg", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, mt6370_dbg_id);

static struct platform_driver mt6370_dbg_driver = {
	.driver = {
		.name = "mt6370_dbg",
		.of_match_table = mt6370_dbg_of_id,
	},
	.probe = mt6370_dbg_probe,
	.remove = mt6370_dbg_remove,
	.id_table = mt6370_dbg_id,
};
module_platform_driver(mt6370_dbg_driver);

MODULE_AUTHOR("cy_huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6370 debug driver");
MODULE_LICENSE("GPL");
