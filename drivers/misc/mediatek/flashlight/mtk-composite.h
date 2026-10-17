/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_COMPOSITE_H
#define _MTK_COMPOSITE_H

#include <linux/list.h>
#include <linux/leds.h>
#include <linux/led-class-flash.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/media-device.h>

#define MISC_MAX_SUBDEVS		8

struct mtk_composite_async_subdev {
	struct v4l2_async_connection asc;
	struct v4l2_subdev *sd;
};

struct mtk_composite_v4l2_device {
	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier;
	struct media_device media_dev;
	struct video_device *vdev;
	struct device *dev;
	u32 revision;

	struct v4l2_subdev **sd;
	struct mtk_composite_async_subdev *asc[MISC_MAX_SUBDEVS];
	struct v4l2_subdev *subdevs[MISC_MAX_SUBDEVS];
};

#endif /* _MTK_COMPOSITE_H */
