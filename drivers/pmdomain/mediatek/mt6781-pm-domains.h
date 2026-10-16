// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SOC_MEDIATEK_MT6781_PM_DOMAINS_H
#define __SOC_MEDIATEK_MT6781_PM_DOMAINS_H

#include "mtk-pm-domains.h"
#include <dt-bindings/power/mt6781-power.h>

#define MT6781_TOP_AXI_PROT_EN_SET	0x2a0
#define MT6781_TOP_AXI_PROT_EN_CLR	0x2a4
#define MT6781_TOP_AXI_PROT_EN_STA	0x220
#define MT6781_TOP_AXI_PROT_EN_1_SET	0x2a8
#define MT6781_TOP_AXI_PROT_EN_1_CLR	0x2ac
#define MT6781_TOP_AXI_PROT_EN_1_STA	0x250

static const struct scpsys_domain_data scpsys_domain_data_mt6781[] = {
	[MT6781_POWER_DOMAIN_MD] = {
		.name = "md",
		.sta_mask = BIT(0),
		.ctl_offs = 0x300,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT6781_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0x304,
		.bp_infracfg = {
			BUS_PROT_WR_IGN(BIT(18),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
			BUS_PROT_WR_IGN(BIT(14),
					MT6781_TOP_AXI_PROT_EN_SET,
					MT6781_TOP_AXI_PROT_EN_CLR,
					MT6781_TOP_AXI_PROT_EN_STA),
		},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT6781_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = BIT(21),
		.ctl_offs = 0x354,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR_IGN(BIT(11) | BIT(12),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
			BUS_PROT_WR_IGN(BIT(1) | BIT(2) | BIT(10) | BIT(11),
					MT6781_TOP_AXI_PROT_EN_SET,
					MT6781_TOP_AXI_PROT_EN_CLR,
					MT6781_TOP_AXI_PROT_EN_STA),
		},
	},
	[MT6781_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.sta_mask = BIT(3),
		.ctl_offs = 0x30c,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT6781_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = BIT(13),
		.ctl_offs = 0x334,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR_IGN(BIT(23),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
			BUS_PROT_WR_IGN(BIT(15),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
		},
	},
	[MT6781_POWER_DOMAIN_VEN] = {
		.name = "ven",
		.sta_mask = BIT(18),
		.ctl_offs = 0x348,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR_IGN(BIT(31),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
			BUS_PROT_WR_IGN(BIT(19),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
		},
	},
	[MT6781_POWER_DOMAIN_MFG0] = {
		.name = "mfg0",
		.sta_mask = BIT(2),
		.ctl_offs = 0x308,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT6781_POWER_DOMAIN_CAM] = {
		.name = "cam",
		.sta_mask = BIT(23),
		.ctl_offs = 0x35c,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR_IGN(BIT(21) | BIT(22),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
			BUS_PROT_WR_IGN(BIT(13) | BIT(14),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
		},
	},
	[MT6781_POWER_DOMAIN_MFG3] = {
		.name = "mfg3",
		.sta_mask = BIT(5),
		.ctl_offs = 0x314,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT6781_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.sta_mask = BIT(4),
		.ctl_offs = 0x310,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT6781_POWER_DOMAIN_ISP2] = {
		.name = "isp2",
		.sta_mask = BIT(14),
		.ctl_offs = 0x338,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT6781_POWER_DOMAIN_VDE] = {
		.name = "vde",
		.sta_mask = BIT(16),
		.ctl_offs = 0x340,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR_IGN(BIT(30),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
			BUS_PROT_WR_IGN(BIT(17),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
		},
	},
	[MT6781_POWER_DOMAIN_IPE] = {
		.name = "ipe",
		.sta_mask = BIT(15),
		.ctl_offs = 0x33c,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR_IGN(BIT(24),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
			BUS_PROT_WR_IGN(BIT(16),
					MT6781_TOP_AXI_PROT_EN_1_SET,
					MT6781_TOP_AXI_PROT_EN_1_CLR,
					MT6781_TOP_AXI_PROT_EN_1_STA),
		},
	},
	[MT6781_POWER_DOMAIN_CAM_RAWA] = {
		.name = "cam_rawa",
		.sta_mask = BIT(24),
		.ctl_offs = 0x360,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT6781_POWER_DOMAIN_CAM_RAWB] = {
		.name = "cam_rawb",
		.sta_mask = BIT(25),
		.ctl_offs = 0x364,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT6781_POWER_DOMAIN_CSI] = {
		.name = "csi",
		.sta_mask = BIT(6),
		.ctl_offs = 0x318,
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
};

static const struct scpsys_soc_data mt6781_scpsys_data = {
	.domains_data = scpsys_domain_data_mt6781,
	.num_domains = ARRAY_SIZE(scpsys_domain_data_mt6781),
};

#endif /* __SOC_MEDIATEK_MT6781_PM_DOMAINS_H */
