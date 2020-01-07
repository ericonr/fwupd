/*
 * Copyright (C) 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "fu-novatek-common.h"

const gchar *
fu_novatek_strerror (guint8 error_enum)
{
	if (error_enum == 0)
		return "Success";
	return NULL;
}
