/*
 * Copyright (C) 2018-2020 Evan Lojewski
 * Copyright (C) 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_MMAN_H
#include <sys/mman.h>
#endif

#ifdef HAVE_VALGRIND
#include <valgrind.h>
#endif /* HAVE_VALGRIND */

#include "fu-common.h"

#include "fu-bcm57xx-common.h"
#include "fu-bcm57xx-mmap-device.h"
#include "fu-bcm57xx-firmware.h"

/* offsets into BAR[0] */
#define REG_DEVICE_PCI_VENDOR_DEVICE_ID		0x6434
#define REG_NVM_SOFTWARE_ARBITRATION		0x7020
#define REG_NVM_ACCESS				0x7024
#define REG_NVM_COMMAND				0x7000
#define REG_NVM_ADDR				0x700c
#define REG_NVM_READ				0x7010
#define REG_NVM_WRITE				0x7008

/* offsets into BAR[2] */
#define REG_APE_MODE				0x10000

typedef struct {
	guint8	*buf;
	gsize	 bufsz;
} FuBcm57xxMmap;

#define FU_BCM57XX_BAR_DEVICE	0
#define FU_BCM57XX_BAR_APE	1
#define FU_BCM57XX_BAR_MAX	3

struct _FuBcm57xxMmapDevice {
	FuUdevDevice		 parent_instance;
	FuBcm57xxMmap		 bar[FU_BCM57XX_BAR_MAX];
};

typedef union {
	guint32 r32;
	struct {
		guint32 reserved_0_0		: 1;
		guint32 Reset			: 1;
		guint32 reserved_2_2		: 1;
		guint32 Done			: 1;
		guint32 Doit			: 1;
		guint32 Wr			: 1;
		guint32 Erase			: 1;
		guint32 First			: 1;
		guint32 Last			: 1;
		guint32 reserved_15_9		: 7;
		guint32 WriteEnableCommand	: 1;
		guint32 WriteDisableCommand	: 1;
		guint32 reserved_31_18		: 14;
	} __attribute__((packed)) bits;
} RegNVMCommand_t;

typedef union {
	guint32 r32;
	struct {
		guint32 ReqSet0			: 1;
		guint32 ReqSet1			: 1;
		guint32 ReqSet2			: 1;
		guint32 ReqSet3			: 1;
		guint32 ReqClr0			: 1;
		guint32 ReqClr1			: 1;
		guint32 ReqClr2			: 1;
		guint32 ReqClr3			: 1;
		guint32 ArbWon0			: 1;
		guint32 ArbWon1			: 1;
		guint32 ArbWon2			: 1;
		guint32 ArbWon3			: 1;
		guint32 Req0			: 1;
		guint32 Req1			: 1;
		guint32 Req2			: 1;
		guint32 Req3			: 1;
		guint32 reserved_31_16		: 16;
	} __attribute__((packed)) bits;
} RegNVMSoftwareArbitration_t;

typedef union {
	guint32 r32;
	struct {
		guint32 Enable			: 1;
		guint32 WriteEnable		: 1;
		guint32 reserved_31_2		: 30;
	} __attribute__((packed)) bits;
} RegNVMAccess_t;

typedef union {
	guint32 r32;
	struct {
		guint32 Reset			: 1;
		guint32 Halt			: 1;
		guint32 FastBoot		: 1;
		guint32 HostDiag		: 1;
		guint32 reserved_4_4		: 1;
		guint32 Event1			: 1;
		guint32 Event2			: 1;
		guint32 GRCint			: 1;
		guint32 reserved_8_8		: 1;
		guint32 SwapATBdword		: 1;
		guint32 reserved_10_10		: 1;
		guint32 SwapARBdword		: 1;
		guint32 reserved_13_12		: 2;
		guint32 Channel0Enable		: 1;
		guint32 Channel2Enable		: 1;
		guint32 reserved_17_16		: 2;
		guint32 MemoryECC		: 1;
		guint32 ICodePIPRdDisable	: 1;
		guint32 reserved_29_20		: 10;
		guint32 Channel1Enable		: 1;
		guint32 Channel3Enable		: 1;
	} __attribute__((packed)) bits;
} RegAPEMode_t;

G_DEFINE_TYPE (FuBcm57xxMmapDevice, fu_bcm57xx_mmap_device, FU_TYPE_UDEV_DEVICE)

#ifdef __ppc64__
#define BARRIER()	__asm__ volatile ("sync 0\neieio\n" : : : "memory")
#else
#define BARRIER()	__asm__ volatile ("" : : : "memory");
#endif

static gboolean
fu_bcm57xx_mmap_device_bar_read (FuBcm57xxMmapDevice *self,
				 guint bar, gsize offset, guint32 *val,
				 GError **error)
{
	guint8 *base = self->bar[bar].buf + offset;

	/* this should never happen */
	if (self->bar[bar].buf == NULL) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "BAR[%u] is not mapped!", bar);
		return FALSE;
	}

	BARRIER();
	*val = *(guint32 *)base;
	return TRUE;
}

static gboolean
fu_bcm57xx_mmap_device_bar_write (FuBcm57xxMmapDevice *self,
				  guint bar, gsize offset, guint32 val,
				  GError **error)
{
	guint8 *base = self->bar[bar].buf + offset;

	/* this should never happen */
	if (self->bar[bar].buf == NULL) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "BAR[%u] is not mapped!", bar);
		return FALSE;
	}

	BARRIER();
	*(guint32 *)base = val;
	BARRIER();
	return TRUE;
}

static gboolean
fu_bcm57xx_mmap_device_nvram_disable (FuBcm57xxMmapDevice *self, GError **error)
{
	RegNVMAccess_t tmp;
	if (!fu_bcm57xx_mmap_device_bar_read (self, FU_BCM57XX_BAR_DEVICE,
					      REG_NVM_ACCESS, &tmp.r32, error))
		return FALSE;
	tmp.bits.Enable = FALSE;
	tmp.bits.WriteEnable = FALSE;
	return fu_bcm57xx_mmap_device_bar_write (self, FU_BCM57XX_BAR_DEVICE,
						 REG_NVM_ACCESS, tmp.r32, error);
}

static gboolean
fu_bcm57xx_mmap_device_nvram_enable (FuBcm57xxMmapDevice *self, GError **error)
{
	RegNVMAccess_t tmp;
	if (!fu_bcm57xx_mmap_device_bar_read (self, FU_BCM57XX_BAR_DEVICE,
					      REG_NVM_ACCESS, &tmp.r32, error))
		return FALSE;
	tmp.bits.Enable = TRUE;
	tmp.bits.WriteEnable = FALSE;
	return fu_bcm57xx_mmap_device_bar_write (self, FU_BCM57XX_BAR_DEVICE,
						 REG_NVM_ACCESS, tmp.r32, error);
}

static gboolean
fu_bcm57xx_mmap_device_nvram_acquire_lock (FuBcm57xxMmapDevice *self, GError **error)
{
	RegNVMSoftwareArbitration_t tmp = { 0 };
	g_autoptr(GTimer) timer = g_timer_new ();
	tmp.bits.ReqSet1 = 1;
	if (!fu_bcm57xx_mmap_device_bar_write (self, FU_BCM57XX_BAR_DEVICE,
					       REG_NVM_SOFTWARE_ARBITRATION, tmp.r32, error))
		return FALSE;
	do {
		if (!fu_bcm57xx_mmap_device_bar_read (self,
						      FU_BCM57XX_BAR_DEVICE,
						      REG_NVM_SOFTWARE_ARBITRATION,
						      &tmp.r32, error))
			return FALSE;
		if (tmp.bits.ArbWon1)
			return TRUE;
		if (g_timer_elapsed (timer, NULL) > 0.2)
			break;
	} while (TRUE);

	/* timed out */
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_TIMED_OUT,
			     "timed out trying to acquire lock #1");
	return FALSE;
}

static gboolean
fu_bcm57xx_mmap_device_nvram_release_lock (FuBcm57xxMmapDevice *self, GError **error)
{
	RegNVMSoftwareArbitration_t tmp = { 0 };
	tmp.r32 = 0;
	tmp.bits.ReqClr1 = 1;
	return fu_bcm57xx_mmap_device_bar_write (self, FU_BCM57XX_BAR_DEVICE,
						 REG_NVM_SOFTWARE_ARBITRATION, tmp.r32,
						 error);
}

static gboolean
fu_bcm57xx_mmap_device_detach (FuDevice *device, GError **error)
{
	/* unbind tg3 */
	return fu_device_unbind_driver (device, error);
}

static gboolean
fu_bcm57xx_mmap_device_attach (FuDevice *device, GError **error)
{
	/* bind tg3 */
	return fu_device_bind_driver (device, "pci", "tg3", error);
}

static gboolean
fu_bcm57xx_mmap_device_activate (FuDevice *device, GError **error)
{
	RegAPEMode_t mode = { 0 };
	FuBcm57xxMmapDevice *self = FU_BCM57XX_MMAP_DEVICE (device);
	g_autoptr(FuDeviceLocker) locker = NULL;
	g_autoptr(FuDeviceLocker) locker2 = NULL;

	locker = fu_device_locker_new_full (self,
					    (FuDeviceLockerFunc) fu_bcm57xx_mmap_device_nvram_acquire_lock,
					    (FuDeviceLockerFunc) fu_bcm57xx_mmap_device_nvram_release_lock,
					    error);
	if (locker == NULL)
		return FALSE;
	locker2 = fu_device_locker_new_full (self,
					     (FuDeviceLockerFunc) fu_bcm57xx_mmap_device_nvram_enable,
					     (FuDeviceLockerFunc) fu_bcm57xx_mmap_device_nvram_disable,
					     error);
	if (locker2 == NULL)
		return FALSE;

	/* halt */
	mode.bits.Halt = 1;
	mode.bits.FastBoot = 0;
	if (!fu_bcm57xx_mmap_device_bar_write (self, FU_BCM57XX_BAR_APE,
					       REG_APE_MODE, mode.r32, error))
		return FALSE;

	/* boot */
	mode.bits.Halt = 0;
	mode.bits.FastBoot = 0;
	mode.bits.Reset = 1;
	return fu_bcm57xx_mmap_device_bar_write (self, FU_BCM57XX_BAR_APE,
					         REG_APE_MODE, mode.r32, error);
}

static gboolean
fu_bcm57xx_mmap_device_open (FuDevice *device, GError **error)
{
	FuBcm57xxMmapDevice *self = FU_BCM57XX_MMAP_DEVICE (device);
#ifdef HAVE_MMAN_H
	FuUdevDevice *udev_device = FU_UDEV_DEVICE (device);
	const gchar *sysfs_path = fu_udev_device_get_sysfs_path (udev_device);
#endif

#ifdef RUNNING_ON_VALGRIND
	/* this can't work */
	if (RUNNING_ON_VALGRIND) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "cannot mmap'ing BARs when using valgrind");
		return FALSE;
	}
#endif

#ifdef HAVE_MMAN_H
	/* map BARs */
	for (guint i = 0; i < FU_BCM57XX_BAR_MAX; i++) {
		int memfd;
		struct stat st;
		g_autofree gchar *fn = NULL;
		g_autofree gchar *resfn = NULL;

		/* open 64 bit resource */
		resfn = g_strdup_printf ("resource%u", i * 2);
		fn = g_build_filename (sysfs_path, resfn, NULL);
		memfd = open (fn, O_RDWR | O_SYNC);
		if (memfd < 0) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_FOUND,
				     "error opening %s", fn);
			return FALSE;
		}
		if (fstat (memfd, &st) < 0) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "could not stat %s", fn);
			close (memfd);
			return FALSE;
		}

		/* mmap */
		g_debug ("mapping BAR[%u] %s for 0x%x bytes", i, fn, (guint) st.st_size);
		self->bar[i].buf = (guint8 *) mmap (0, st.st_size,
						    PROT_READ | PROT_WRITE,
						    MAP_SHARED, memfd, 0);
		self->bar[i].bufsz = st.st_size;
		close (memfd);
		if (self->bar[i].buf == MAP_FAILED) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "cound not mmap %s: %s",
				     fn, strerror(errno));
			return FALSE;
		}
	}
#else
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "mmap() not supported as sys/mman.h not available");
	return FALSE;
#endif

	/* success */
	return TRUE;
}

static gboolean
fu_bcm57xx_mmap_device_close (FuDevice *device, GError **error)
{
	FuBcm57xxMmapDevice *self = FU_BCM57XX_MMAP_DEVICE (device);

#ifdef HAVE_MMAN_H
	/* unmap BARs */
	for (guint i = 0; i < FU_BCM57XX_BAR_MAX; i++) {
		if (self->bar[i].buf == NULL)
			continue;
		g_debug ("unmapping BAR[%u]", i);
		munmap (self->bar[i].buf, self->bar[i].bufsz);
		self->bar[i].buf = NULL;
		self->bar[i].bufsz = 0;
	}
#endif

	/* success */
	return TRUE;
}

static void
fu_bcm57xx_mmap_device_init (FuBcm57xxMmapDevice *self)
{
	/* no BARs mapped */
	for (guint i = 0; i < FU_BCM57XX_BAR_MAX; i++) {
		self->bar[i].buf = NULL;
		self->bar[i].bufsz = 0;
	}
}

static void
fu_bcm57xx_mmap_device_class_init (FuBcm57xxMmapDeviceClass *klass)
{
	FuDeviceClass *klass_device = FU_DEVICE_CLASS (klass);
	klass_device->activate = fu_bcm57xx_mmap_device_activate;
	klass_device->open = fu_bcm57xx_mmap_device_open;
	klass_device->close = fu_bcm57xx_mmap_device_close;
	klass_device->attach = fu_bcm57xx_mmap_device_attach;
	klass_device->detach = fu_bcm57xx_mmap_device_detach;
}

FuBcm57xxMmapDevice *
fu_bcm57xx_mmap_device_new (GUdevDevice *udev_device)
{
	FuUdevDevice *self = g_object_new (FU_TYPE_BCM57XX_MMAP_DEVICE,
					   "udev-device", udev_device,
					   NULL);
	return FU_BCM57XX_MMAP_DEVICE (self);
}
