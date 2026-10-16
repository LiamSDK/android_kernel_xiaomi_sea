// SPDX-License-Identifier: GPL-2.0
/*
 * Extdev IO Class
 *
 * Copyright (C) Richtek Technology Corp.
 * Author: Gene Chen <gene_chen@richtek.com>
 */

#include <linux/device.h>
#include <linux/extdev_io_class.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

static struct class *extdev_io_class;

#define PREALLOC_WBUFFER_SIZE 1000

static ssize_t extdev_io_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct extdev_io_dev *extdev = dev_get_drvdata(dev);
	const char *name = attr->attr.name;
	int ret = 0, i;
	u8 *pdata;

	if (!extdev)
		return -ENODEV;

	if (!strcmp(name, "reg")) {
		return sprintf(buf, "0x%04x\n", extdev->reg);
	} else if (!strcmp(name, "size")) {
		return sprintf(buf, "%d\n", extdev->size);
	} else if (!strcmp(name, "type")) {
		return sprintf(buf, "%s\n", extdev->typestr ? extdev->typestr : "");
	} else if (!strcmp(name, "lock")) {
		return sprintf(buf, "%d\n", extdev->access_lock);
	} else if (!strcmp(name, "data")) {
		mutex_lock(&extdev->io_lock);
		if (extdev->data_buffer_size < extdev->size) {
			void *buffer = kzalloc(extdev->size, GFP_KERNEL);
			if (!buffer) {
				mutex_unlock(&extdev->io_lock);
				return -ENOMEM;
			}
			kfree(extdev->data_buffer);
			extdev->data_buffer = buffer;
			extdev->data_buffer_size = extdev->size;
		}

		if (!extdev->io_read) {
			mutex_unlock(&extdev->io_lock);
			return -EPERM;
		}

		ret = extdev->io_read(extdev->io_drvdata, extdev->reg,
				      extdev->data_buffer, extdev->size);
		if (ret < 0) {
			mutex_unlock(&extdev->io_lock);
			return ret;
		}

		pdata = extdev->data_buffer;
		ret = sprintf(buf, "0x");
		for (i = 0; i < extdev->size; i++)
			ret += sprintf(buf + ret, "%02x,", *(pdata + i));
		ret += sprintf(buf + ret, "\n");
		mutex_unlock(&extdev->io_lock);
		return ret;
	}

	return -EINVAL;
}

static ssize_t extdev_io_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct extdev_io_dev *extdev = dev_get_drvdata(dev);
	const char *name = attr->attr.name;
	int ret = 0, val_cnt = 0;
	char kbuf[PREALLOC_WBUFFER_SIZE + 1];
	char *token, *cur;
	void *buffer;
	u8 *pdata;
	u64 val;

	if (!extdev)
		return -ENODEV;

	if (!strcmp(name, "reg")) {
		ret = kstrtoull(buf, 0, &val);
		if (ret)
			return ret;
		extdev->reg = (u16)val;
		return count;
	} else if (!strcmp(name, "size")) {
		ret = kstrtoull(buf, 0, &val);
		if (ret)
			return ret;
		extdev->size = (u16)val;
		return count;
	} else if (!strcmp(name, "lock")) {
		ret = kstrtoull(buf, 0, &val);
		if (ret)
			return ret;
		extdev->access_lock = (bool)val;
		return count;
	} else if (!strcmp(name, "data")) {
		if (count > PREALLOC_WBUFFER_SIZE)
			return -ENOMEM;
		memcpy(kbuf, buf, count);
		kbuf[count] = '\0';

		mutex_lock(&extdev->io_lock);
		if (extdev->data_buffer_size < extdev->size) {
			buffer = kzalloc(extdev->size, GFP_KERNEL);
			if (!buffer) {
				mutex_unlock(&extdev->io_lock);
				return -ENOMEM;
			}
			kfree(extdev->data_buffer);
			extdev->data_buffer = buffer;
			extdev->data_buffer_size = extdev->size;
		}

		pdata = extdev->data_buffer;
		cur = kbuf;
		while ((token = strsep(&cur, " ,\n")) != NULL) {
			u8 byte_val;
			if (!*token)
				continue;
			if (val_cnt >= extdev->size)
				break;
			ret = kstrtou8(token, 0, &byte_val);
			if (ret) {
				mutex_unlock(&extdev->io_lock);
				return ret;
			}
			pdata[val_cnt++] = byte_val;
		}

		if (!extdev->io_write) {
			mutex_unlock(&extdev->io_lock);
			return -EPERM;
		}

		ret = extdev->io_write(extdev->io_drvdata, extdev->reg,
				       extdev->data_buffer, val_cnt);
		mutex_unlock(&extdev->io_lock);
		if (ret < 0)
			return ret;
		return count;
	}

	return -EINVAL;
}

static DEVICE_ATTR(reg, 0644, extdev_io_show, extdev_io_store);
static DEVICE_ATTR(size, 0644, extdev_io_show, extdev_io_store);
static DEVICE_ATTR(data, 0644, extdev_io_show, extdev_io_store);
static DEVICE_ATTR(type, 0444, extdev_io_show, NULL);
static DEVICE_ATTR(lock, 0644, extdev_io_show, extdev_io_store);

static struct attribute *extdev_io_device_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_size.attr,
	&dev_attr_data.attr,
	&dev_attr_type.attr,
	&dev_attr_lock.attr,
	NULL,
};

static const struct attribute_group extdev_io_attr_group = {
	.attrs = extdev_io_device_attributes,
};

static const struct attribute_group *extdev_io_attr_groups[] = {
	&extdev_io_attr_group,
	NULL,
};

struct extdev_io_dev *extdev_io_device_register(struct device *parent,
						const struct extdev_io_desc *desc)
{
	struct extdev_io_dev *extdev;

	if (!parent) {
		pr_err("%s: Expected proper parent device\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	extdev = kzalloc(sizeof(*extdev), GFP_KERNEL);
	if (!extdev) {
		pr_err("%s: failed to alloc extdev\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	extdev->parent = parent;
	extdev->devname = desc->devname;
	extdev->typestr = desc->typestr;
	extdev->io_read = desc->io_read;
	extdev->io_write = desc->io_write;
	extdev->io_drvdata = desc->io_drvdata;
	mutex_init(&extdev->io_lock);

	extdev->dev = device_create_with_groups(extdev_io_class, parent,
						MKDEV(0, 0), extdev,
						extdev_io_attr_groups,
						"%s", desc->devname);
	if (IS_ERR(extdev->dev)) {
		int ret = PTR_ERR(extdev->dev);
		kfree(extdev);
		return ERR_PTR(ret);
	}

	return extdev;
}
EXPORT_SYMBOL_GPL(extdev_io_device_register);

void extdev_io_device_unregister(struct extdev_io_dev *extdev)
{
	if (!extdev)
		return;

	if (extdev->dev)
		device_unregister(extdev->dev);
	kfree(extdev->data_buffer);
	kfree(extdev);
}
EXPORT_SYMBOL_GPL(extdev_io_device_unregister);

static void devm_extdev_io_device_release(struct device *dev, void *res)
{
	struct extdev_io_dev **pdev = res;
	extdev_io_device_unregister(*pdev);
}

struct extdev_io_dev *devm_extdev_io_device_register(struct device *parent,
						     const struct extdev_io_desc *desc)
{
	struct extdev_io_dev **ptr, *extdev;

	ptr = devres_alloc(devm_extdev_io_device_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	extdev = extdev_io_device_register(parent, desc);
	if (!IS_ERR(extdev)) {
		*ptr = extdev;
		devres_add(parent, ptr);
	} else {
		devres_free(ptr);
	}

	return extdev;
}
EXPORT_SYMBOL_GPL(devm_extdev_io_device_register);

static int __init extdev_io_class_init(void)
{
	extdev_io_class = class_create("extdev_io");
	if (IS_ERR(extdev_io_class)) {
		pr_err("Unable to create extdev_io class(%ld)\n",
		       PTR_ERR(extdev_io_class));
		return PTR_ERR(extdev_io_class);
	}

	pr_info("extdev_io class init OK\n");
	return 0;
}

static void __exit extdev_io_class_exit(void)
{
	class_destroy(extdev_io_class);
	pr_info("extdev_io class deinit OK\n");
}

module_init(extdev_io_class_init);
module_exit(extdev_io_class_exit);

MODULE_AUTHOR("Gene Chen <gene_chen@richtek.com>");
MODULE_DESCRIPTION("Extdev io class");
MODULE_LICENSE("GPL");
