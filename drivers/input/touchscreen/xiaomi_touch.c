// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021-2024 Xiaomi Inc.
 * xiaomi Touchscreen Driver
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/input/xiaomi_touch.h>

static struct class *xiaomi_touch_class;
static struct device *xiaomi_touch_dev;
static struct xiaomi_touch_pdata *touch_pdata[TOUCH_MAX_NUM];
static DEFINE_MUTEX(touch_mutex);
static int palm_sensor_value = 0;
static int touch_dfs_test_val = 0;

struct class *get_xiaomi_touch_class(void)
{
	return xiaomi_touch_class;
}
EXPORT_SYMBOL(get_xiaomi_touch_class);

struct device *get_xiaomi_touch_dev(void)
{
	return xiaomi_touch_dev;
}
EXPORT_SYMBOL(get_xiaomi_touch_dev);

struct device *xiaomi_touch_dev_get(int touch_id)
{
	if (touch_id < 0 || touch_id >= TOUCH_MAX_NUM)
		return NULL;
	return xiaomi_touch_dev;
}
EXPORT_SYMBOL(xiaomi_touch_dev_get);

int update_palm_sensor_value(int val)
{
	mutex_lock(&touch_mutex);
	palm_sensor_value = val;
	if (xiaomi_touch_dev)
		sysfs_notify(&xiaomi_touch_dev->kobj, NULL, "palm_sensor");
	mutex_unlock(&touch_mutex);
	return 0;
}
EXPORT_SYMBOL(update_palm_sensor_value);

int xiaomitouch_register_modedata(struct xiaomi_touch_pdata *data)
{
	if (!data)
		return -EINVAL;

	mutex_lock(&touch_mutex);
	if (data->touch_id >= 0 && data->touch_id < TOUCH_MAX_NUM)
		touch_pdata[data->touch_id] = data;
	mutex_unlock(&touch_mutex);

	return 0;
}
EXPORT_SYMBOL(xiaomitouch_register_modedata);

int xiaomi_touch_mievent_report_int(int touch_id, const char *key, int val)
{
	return 0;
}
EXPORT_SYMBOL_GPL(xiaomi_touch_mievent_report_int);

int xiaomi_touch_mievent_report_str(int touch_id, const char *key, const char *val)
{
	return 0;
}
EXPORT_SYMBOL_GPL(xiaomi_touch_mievent_report_str);

static ssize_t palm_sensor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", palm_sensor_value);
}

static ssize_t palm_sensor_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	if (sscanf(buf, "%d", &val) == 1) {
		update_palm_sensor_value(val);
	}
	return count;
}
static DEVICE_ATTR_RW(palm_sensor);

static ssize_t touch_dfs_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", touch_dfs_test_val);
}

static ssize_t touch_dfs_test_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	if (sscanf(buf, "%d", &val) == 1) {
		touch_dfs_test_val = val;
	}
	return count;
}
static DEVICE_ATTR_RW(touch_dfs_test);

static struct attribute *touch_attrs[] = {
	&dev_attr_palm_sensor.attr,
	&dev_attr_touch_dfs_test.attr,
	NULL,
};

static const struct attribute_group touch_attr_group = {
	.attrs = touch_attrs,
};

static int xiaomi_touch_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int xiaomi_touch_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long xiaomi_touch_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct file_operations xiaomitouch_dev_fops = {
	.owner          = THIS_MODULE,
	.open           = xiaomi_touch_dev_open,
	.release        = xiaomi_touch_dev_release,
	.unlocked_ioctl = xiaomi_touch_dev_ioctl,
	.compat_ioctl   = xiaomi_touch_dev_ioctl,
};

static struct miscdevice xiaomi_touch_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "xiaomi-touch",
	.fops  = &xiaomitouch_dev_fops,
};

static int xiaomi_touch_probe(struct platform_device *pdev)
{
	int ret;

	ret = misc_register(&xiaomi_touch_misc);
	if (ret)
		return ret;

	xiaomi_touch_class = class_create("xiaomi-touch");
	if (IS_ERR(xiaomi_touch_class)) {
		misc_deregister(&xiaomi_touch_misc);
		return PTR_ERR(xiaomi_touch_class);
	}

	xiaomi_touch_dev = device_create(xiaomi_touch_class, NULL, 0, NULL, "touch_dev");
	if (IS_ERR(xiaomi_touch_dev)) {
		class_destroy(xiaomi_touch_class);
		misc_deregister(&xiaomi_touch_misc);
		return PTR_ERR(xiaomi_touch_dev);
	}

	ret = sysfs_create_group(&xiaomi_touch_dev->kobj, &touch_attr_group);
	if (ret) {
		device_destroy(xiaomi_touch_class, 0);
		class_destroy(xiaomi_touch_class);
		misc_deregister(&xiaomi_touch_misc);
		return ret;
	}

	platform_set_drvdata(pdev, xiaomi_touch_dev);
	return 0;
}

static int xiaomi_touch_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&xiaomi_touch_dev->kobj, &touch_attr_group);
	device_destroy(xiaomi_touch_class, 0);
	class_destroy(xiaomi_touch_class);
	misc_deregister(&xiaomi_touch_misc);
	return 0;
}

static const struct of_device_id xiaomi_touch_of_match[] = {
	{ .compatible = "xiaomi-touch" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, xiaomi_touch_of_match);

static struct platform_driver xiaomi_touch_device_driver = {
	.probe    = xiaomi_touch_probe,
	.remove   = xiaomi_touch_remove,
	.driver   = {
		.name           = "xiaomi-touch",
		.of_match_table = xiaomi_touch_of_match,
	},
};

module_platform_driver(xiaomi_touch_device_driver);

MODULE_DESCRIPTION("xiaomi Touchscreen Driver");
MODULE_LICENSE("GPL");
