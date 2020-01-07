/*
 * Copyright (C) 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "fu-plugin.h"

#define FU_TYPE_NOVATEK_DEVICE (fu_novatek_device_get_type ())
G_DECLARE_FINAL_TYPE (FuNovatekDevice, fu_novatek_device, FU, NOVATEK_DEVICE, FuUsbDevice)
