# SPDX-License-Identifier: GPL-2.0
# Makefile for the focaltech ft3418_i2c touchscreen driver

obj-$(CONFIG_TOUCHSCREEN_FT3418_I2C) += ft3418_i2c.o

ft3418_i2c-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o \
		focaltech_flash.o \
		focaltech_flash/focaltech_upgrade_ft3418.o \
		focaltech_test/focaltech_test.o \
		focaltech_test/focaltech_test_ini.o \
		focaltech_test/supported_ic/focaltech_test_ft3418.o \
		focaltech_i2c.o
