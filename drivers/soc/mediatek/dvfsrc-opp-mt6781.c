// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2024 MediaTek Inc.
 * DVFSRC SPMFW INIT for MT6781
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/arm-smccc.h>

extern unsigned int mtk_dramc_get_steps_freq(unsigned int step);

static const unsigned int step_table[] = {0, 1, 3, 5, 7, 9};

int ddr_level_to_step(int opp)
{
	if (opp < 0 || opp >= ARRAY_SIZE(step_table))
		return 0;
	return step_table[opp];
}
EXPORT_SYMBOL(ddr_level_to_step);

static int __init dvfsrc_opp_mt6781_init(void)
{
	struct arm_smccc_res res;
	int i;

	for (i = 0; i < ARRAY_SIZE(step_table); i++) {
		unsigned int freq = mtk_dramc_get_steps_freq(step_table[i]);

		arm_smccc_smc(0xc2000506, 0x10, i, freq, 0, 0, 0, 0, &res);
	}

	return 0;
}

module_init(dvfsrc_opp_mt6781_init);

MODULE_DESCRIPTION("DVFSRC SPMFW INIT");
MODULE_LICENSE("GPL");
