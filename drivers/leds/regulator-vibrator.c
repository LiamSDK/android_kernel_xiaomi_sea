// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019-2024 MediaTek Inc.
 *
 * Regulator Vibrator Driver (VIB)
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#define VIB_DEVICE "regulator_vibrator"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME " %s(%d) :" fmt, __func__, __LINE__

struct reg_vibr_config {
	unsigned int min_volt;
	unsigned int max_volt;
	struct regulator *reg;
	const char *label;
};

struct reg_vibr {
	struct device *dev;
	struct workqueue_struct *vibr_queue;
	struct work_struct vibr_work;
	spinlock_t vibr_lock;
	bool vibr_state;
	bool vibr_oc_state;
	bool reg_status;
	bool vibr_shutdown;
	struct reg_vibr_config vibr_conf;
	struct notifier_block oc_handle;
	struct led_classdev led_dev;
};

static int mt_vibra_init_config(struct device *dev,
				struct reg_vibr_config *vibr_conf)
{
	int ret;

	ret = of_property_read_string(dev->of_node, "label", &vibr_conf->label);
	if (ret) {
		pr_notice("Error load dts: get regulator label return %d\n", ret);
		vibr_conf->label = "vibrator";
	}

	vibr_conf->reg = devm_regulator_get(dev, "vib");
	if (IS_ERR(vibr_conf->reg)) {
		ret = PTR_ERR(vibr_conf->reg);
		pr_notice("Error load dts: get regulator return %d\n", ret);
		vibr_conf->reg = NULL;
	}

	ret = of_property_read_u32(dev->of_node, "max-volt", &vibr_conf->max_volt);
	if (ret) {
		pr_notice("Error load dts: get max-volt failed!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(dev->of_node, "min-volt", &vibr_conf->min_volt);
	if (ret) {
		pr_notice("Error load dts: get min-volt failed!\n");
		return -EINVAL;
	}

	if (vibr_conf->min_volt > vibr_conf->max_volt) {
		pr_notice("Error load dts: get error voltage(min > max)!\n");
		return -EINVAL;
	}

	pr_info("vibr_conf %u-%u\n", vibr_conf->min_volt, vibr_conf->max_volt);
	return 0;
}

static int vibr_power_set(struct reg_vibr *vibr)
{
	int ret = 0;

	if (!vibr->vibr_conf.reg)
		return ret;

	pr_info("set voltage = %u-%u\n",
		vibr->vibr_conf.min_volt, vibr->vibr_conf.max_volt);
	ret = regulator_set_voltage(vibr->vibr_conf.reg,
				    vibr->vibr_conf.min_volt,
				    vibr->vibr_conf.max_volt);
	if (ret < 0)
		pr_notice("set voltage fail, ret = %d\n", ret);

	return ret;
}

static void vibr_enable(struct reg_vibr *vibr)
{
	if (!vibr->vibr_conf.reg)
		return;

	if (!vibr->reg_status) {
		if (regulator_enable(vibr->vibr_conf.reg)) {
			pr_notice("set vibr_reg enable failed!\n");
		} else {
			vibr->reg_status = true;
			pr_info("vibr enable\n");
		}
	} else {
		pr_notice("vibr_reg already enabled.\n");
	}
}

static void vibr_disable(struct reg_vibr *vibr)
{
	if (!vibr->vibr_conf.reg)
		return;

	if (vibr->reg_status) {
		if (regulator_disable(vibr->vibr_conf.reg)) {
			pr_notice("set vibr_reg disable failed!\n");
		} else {
			vibr->reg_status = false;
			pr_info("vibr disable\n");
		}
	} else {
		pr_notice("vibr_reg already disabled.\n");
	}
}

static void update_vibrator(struct work_struct *work)
{
	struct reg_vibr *vibr = container_of(work, struct reg_vibr, vibr_work);

	if (vibr->vibr_oc_state) {
		vibr->vibr_oc_state = false;
		vibr_disable(vibr);
		return;
	}

	if (!vibr->vibr_state)
		vibr_disable(vibr);
	else
		vibr_enable(vibr);
}

static void regulator_vibrator_set(struct led_classdev *cdev,
				   enum led_brightness value)
{
	struct reg_vibr *vibr = container_of(cdev, struct reg_vibr, led_dev);
	unsigned long flags;

	spin_lock_irqsave(&vibr->vibr_lock, flags);
	if (vibr->vibr_shutdown) {
		spin_unlock_irqrestore(&vibr->vibr_lock, flags);
		return;
	}

	vibr->vibr_state = (value != LED_OFF);
	spin_unlock_irqrestore(&vibr->vibr_lock, flags);
	queue_work(vibr->vibr_queue, &vibr->vibr_work);
}

static int regulator_oc_event(struct notifier_block *nb,
			      unsigned long event, void *data)
{
	struct reg_vibr *vibr = container_of(nb, struct reg_vibr, oc_handle);

	switch (event) {
	case REGULATOR_EVENT_OVER_CURRENT:
	case REGULATOR_EVENT_FAIL:
		pr_info("get regulator oc event: %lu\n", event);
		vibr->vibr_oc_state = true;
		queue_work(vibr->vibr_queue, &vibr->vibr_work);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static int vib_probe(struct platform_device *pdev)
{
	struct reg_vibr *m_vibr;
	int ret;

	pr_info("probe start +++\n");
	m_vibr = devm_kzalloc(&pdev->dev, sizeof(*m_vibr), GFP_KERNEL);
	if (!m_vibr) {
		ret = -ENOMEM;
		goto err;
	}

	m_vibr->dev = &pdev->dev;
	m_vibr->vibr_queue = alloc_workqueue(VIB_DEVICE, WQ_UNBOUND | WQ_HIGHPRI, 1);
	if (!m_vibr->vibr_queue) {
		ret = -ENOMEM;
		pr_notice("unable to create workqueue!\n");
		goto err;
	}

	ret = mt_vibra_init_config(&pdev->dev, &m_vibr->vibr_conf);
	if (ret) {
		pr_notice("failed to parse devicetree(%d)!\n", ret);
		goto err;
	}

	INIT_WORK(&m_vibr->vibr_work, update_vibrator);
	spin_lock_init(&m_vibr->vibr_lock);
	m_vibr->vibr_shutdown = false;
	m_vibr->oc_handle.notifier_call = regulator_oc_event;

	if (m_vibr->vibr_conf.reg) {
		m_vibr->reg_status = regulator_is_enabled(m_vibr->vibr_conf.reg);
		if (devm_regulator_register_notifier(m_vibr->vibr_conf.reg,
						     &m_vibr->oc_handle))
			pr_info("regulator notifier request failed\n");
	} else {
		m_vibr->reg_status = false;
	}

	m_vibr->led_dev.name = m_vibr->vibr_conf.label;
	m_vibr->led_dev.brightness_set = regulator_vibrator_set;
	m_vibr->led_dev.max_brightness = LED_FULL;

	ret = devm_led_classdev_register(&pdev->dev, &m_vibr->led_dev);
	if (ret < 0) {
		pr_info("led class register fail\n");
		goto err;
	}

	platform_set_drvdata(pdev, m_vibr);
	ret = vibr_power_set(m_vibr);
	if (ret < 0) {
		pr_info("set voltage for regulator fail\n");
		goto err;
	}

	pr_info("probe success, end ---\n");
	return 0;

err:
	pr_notice("probe failed(%d), end ---!\n", ret);
	return ret;
}

static int vib_remove(struct platform_device *pdev)
{
	struct reg_vibr *vibr = platform_get_drvdata(pdev);

	cancel_work_sync(&vibr->vibr_work);
	devm_led_classdev_unregister(&pdev->dev, &vibr->led_dev);
	return 0;
}

static void vib_shutdown(struct platform_device *pdev)
{
	struct reg_vibr *vibr = platform_get_drvdata(pdev);
	unsigned long flags;

	pr_info("shutdown: enter!\n");
	spin_lock_irqsave(&vibr->vibr_lock, flags);
	vibr->vibr_shutdown = true;
	if (vibr->vibr_state) {
		vibr->vibr_state = false;
		spin_unlock_irqrestore(&vibr->vibr_lock, flags);
		pr_info("vibrator will disable!\n");
		vibr_disable(vibr);
	} else {
		spin_unlock_irqrestore(&vibr->vibr_lock, flags);
	}
}

static int vib_suspend(struct device *dev)
{
	struct reg_vibr *vibr = dev_get_drvdata(dev);

	if (vibr && vibr->vibr_state) {
		vibr->vibr_state = false;
		vibr_disable(vibr);
		pr_notice("vibr disabled, enter suspend.\n");
	}
	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(vib_pm_ops, vib_suspend, NULL);

static const struct of_device_id vibr_of_ids[] = {
	{ .compatible = "regulator-vibrator", },
	{}
};
MODULE_DEVICE_TABLE(of, vibr_of_ids);

static struct platform_driver vibrator_driver = {
	.probe = vib_probe,
	.remove = vib_remove,
	.shutdown = vib_shutdown,
	.driver = {
		.name = "regulator_vibrator",
		.of_match_table = vibr_of_ids,
		.pm = pm_sleep_ptr(&vib_pm_ops),
	},
};
module_platform_driver(vibrator_driver);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("Regulator Vibrator Driver (VIB)");
MODULE_LICENSE("GPL");
