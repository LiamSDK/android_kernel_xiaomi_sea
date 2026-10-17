# SPDX-License-Identifier: GPL-2.0

export MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
export ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/include/mt-plat

obj-$(CONFIG_MTK_FLASHLIGHT) += flashlight/
obj-$(CONFIG_MTK_IMGSENSOR) += imgsensor/src/
obj-$(CONFIG_MTK_CAM_CAL) += cam_cal/
obj-$(CONFIG_MTK_CAMERA_ISP) += cameraisp/
obj-$(CONFIG_MTK_COMBO) += connectivity/
obj-$(CONFIG_MTK_CCCI_DEVICES) += ccci_util/
obj-$(CONFIG_MTK_ECCCI_DRIVER) += eccci/
obj-$(CONFIG_MTK_ECCCI_C2K) += c2k_usb/
obj-$(CONFIG_MTK_AUDIO_IPI) += audio_ipi/
obj-$(CONFIG_MTK_ELLIPTIC_ULTRASOUND) += elliptic/
obj-y += sensors-1.0/
obj-y += power_throttling/

