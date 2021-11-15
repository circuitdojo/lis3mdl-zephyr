# Copyright (c) 2016 Intel Corporation
# Copyright (c) 2021 Circuit Dojo LLC
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources(lis3mdl.c)
zephyr_library_sources_ifdef(CONFIG_LIS3MDL_TRIGGER lis3mdl_trigger.c)
