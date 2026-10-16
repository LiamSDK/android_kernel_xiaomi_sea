/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2024 MediaTek Inc.
 * Author: Freya Li <freya.li@mediatek.com>
 */
#ifndef __MTK_DISP_NOTIFY_H__
#define __MTK_DISP_NOTIFY_H__

#include <linux/notifier.h>

enum {
	MTK_DISP_EARLY_EVENT_BLANK = 0,
	MTK_DISP_EVENT_BLANK,
	MTK_DISP_EVENT_POWER_ON,
	MTK_DISP_EVENT_POWER_OFF,
};

int mtk_disp_notifier_register(struct notifier_block *nb);
int mtk_disp_notifier_unregister(struct notifier_block *nb);
int mtk_disp_notifier_call_chain(unsigned long val, void *v);

int mtk_disp_sub_notifier_register(struct notifier_block *nb);
int mtk_disp_sub_notifier_unregister(struct notifier_block *nb);
int mtk_disp_sub_notifier_call_chain(unsigned long val, void *v);

int mtk_disp_3rd_notifier_register(struct notifier_block *nb);
int mtk_disp_3rd_notifier_unregister(struct notifier_block *nb);
int mtk_disp_3rd_notifier_call_chain(unsigned long val, void *v);

#endif /* __MTK_DISP_NOTIFY_H__ */
