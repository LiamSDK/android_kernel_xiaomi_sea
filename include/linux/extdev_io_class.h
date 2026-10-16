/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Extdev IO Class
 *
 * Copyright (C) Richtek Technology Corp.
 * Author: Gene Chen <gene_chen@richtek.com>
 */

#ifndef __LINUX_EXTDEV_IO_CLASS_H
#define __LINUX_EXTDEV_IO_CLASS_H

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/types.h>

struct extdev_io_dev;

struct extdev_io_desc {
	const char *devname;
	const char *typestr;
	int (*io_read)(void *drvdata, u16 reg, void *val, u16 size);
	int (*io_write)(void *drvdata, u16 reg, const void *val, u16 size);
	void *io_drvdata;
};

struct extdev_io_dev {
	struct device *dev;
	struct device *parent;
	const char *devname;
	const char *typestr;
	int (*io_read)(void *drvdata, u16 reg, void *val, u16 size);
	int (*io_write)(void *drvdata, u16 reg, const void *val, u16 size);
	void *io_drvdata;
	struct mutex io_lock;
	u16 reg;
	u16 size;
	u16 data_buffer_size;
	void *data_buffer;
	bool access_lock;
};

struct extdev_io_dev *extdev_io_device_register(struct device *parent,
						const struct extdev_io_desc *desc);
void extdev_io_device_unregister(struct extdev_io_dev *extdev);

struct extdev_io_dev *devm_extdev_io_device_register(struct device *parent,
						     const struct extdev_io_desc *desc);

#endif /* __LINUX_EXTDEV_IO_CLASS_H */
