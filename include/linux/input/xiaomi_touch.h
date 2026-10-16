/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021-2024 Xiaomi Inc.
 */

#ifndef __XIAOMI_TOUCH_H__
#define __XIAOMI_TOUCH_H__

#include <linux/device.h>
#include <linux/input.h>
#include <linux/types.h>

#define TOUCH_MAX_NUM 2

struct xiaomi_touch_pdata {
	int touch_id;
	const char *name;
	int (*set_mode_value)(int mode, int value);
	int (*get_mode_value)(int mode, int *value);
	int (*get_mode_mask)(int mode, int *mask);
	int (*reset_mode)(int mode);
	int (*enable_palm_sensor)(bool enable);
	void *data;
};

struct class *get_xiaomi_touch_class(void);
struct device *get_xiaomi_touch_dev(void);
struct device *xiaomi_touch_dev_get(int touch_id);
int update_palm_sensor_value(int val);
int xiaomitouch_register_modedata(struct xiaomi_touch_pdata *data);
int xiaomi_touch_mievent_report_int(int touch_id, const char *key, int val);
int xiaomi_touch_mievent_report_str(int touch_id, const char *key, const char *val);

#endif /* __XIAOMI_TOUCH_H__ */
