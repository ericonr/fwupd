/*
 * Copyright (C) 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <string.h>

#include "fu-chunk.h"
#include "fu-novatek-common.h"
#include "fu-novatek-device.h"

struct _FuNovatekDevice {
	FuUsbDevice		 parent_instance;
};

G_DEFINE_TYPE (FuNovatekDevice, fu_novatek_device, FU_TYPE_USB_DEVICE)

static gboolean
fu_novatek_device_detach (FuDevice *device, GError **error)
{
//	FuNovatekDevice *self = FU_NOVATEK_DEVICE (device);
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_RESTART);
	fu_device_add_flag (device, FWUPD_DEVICE_FLAG_WAIT_FOR_REPLUG);
	return TRUE;
}

static gboolean
fu_novatek_device_attach (FuDevice *device, GError **error)
{
//	FuNovatekDevice *self = FU_NOVATEK_DEVICE (device);
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_RESTART);
	fu_device_add_flag (device, FWUPD_DEVICE_FLAG_WAIT_FOR_REPLUG);
	return TRUE;
}

static gboolean
fu_novatek_device_reload (FuDevice *device, GError **error)
{
//	FuNovatekDevice *self = FU_NOVATEK_DEVICE (device);
	return TRUE;
}

static gboolean
fu_novatek_device_probe (FuUsbDevice *device, GError **error)
{
//	FuNovatekDevice *self = FU_NOVATEK_DEVICE (device);

	/* success */
	return TRUE;
}

static gboolean
fu_novatek_device_open (FuUsbDevice *device, GError **error)
{
	GUsbDevice *usb_device = fu_usb_device_get_dev (device);

	/* got the version using the HID API */
	if (!g_usb_device_set_configuration (usb_device, 0x1, error))
		return FALSE;
	if (!g_usb_device_claim_interface (usb_device, 0x01, //FIXME
					   G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					   error)) {
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gboolean
fu_novatek_device_setup (FuDevice *device, GError **error)
{
//	FuNovatekDevice *self = FU_NOVATEK_DEVICE (device);

	/* success */
	return TRUE;
}

static gboolean
fu_novatek_device_write_firmware (FuDevice *device,
				   FuFirmware *firmware,
				   FwupdInstallFlags flags,
				   GError **error)
{
//	FuNovatekDevice *self = FU_NOVATEK_DEVICE (device);
	g_autoptr(GBytes) fw = NULL;
	g_autoptr(GPtrArray) chunks = NULL;

	/* get default image */
	fw = fu_firmware_get_image_default_bytes (firmware, error);
	if (fw == NULL)
		return FALSE;

	/* build packets */
	chunks = fu_chunk_array_new_from_bytes (fw,
						0x0,
						0x00,	/* page_sz */
						0x1000);	//FIXME
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_WRITE);

	/* success! */
	return TRUE;
}

static void
fu_novatek_device_init (FuNovatekDevice *self)
{
	/* this is the application code */
	fu_device_set_protocol (FU_DEVICE (self), "tw.com.novatek");
	fu_device_set_remove_delay (FU_DEVICE (self),
				    FU_DEVICE_REMOVE_DELAY_RE_ENUMERATE);
}

static void
fu_novatek_device_class_init (FuNovatekDeviceClass *klass)
{
	FuDeviceClass *klass_device = FU_DEVICE_CLASS (klass);
	FuUsbDeviceClass *klass_usb_device = FU_USB_DEVICE_CLASS (klass);
	klass_device->write_firmware = fu_novatek_device_write_firmware;
	klass_device->attach = fu_novatek_device_attach;
	klass_device->detach = fu_novatek_device_detach;
	klass_device->reload = fu_novatek_device_reload;
	klass_device->setup = fu_novatek_device_setup;
	klass_usb_device->open = fu_novatek_device_open;
	klass_usb_device->probe = fu_novatek_device_probe;
}

#if 0
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#include <glib.h>

#define MAX_BINLEN (14*1024)

static libusb_device_handle *devh;
static libusb_context *ctx;
static gint devintf;

extern guint8 firmware_fw_tp_update_hex[];
extern unsigned int firmware_fw_tp_update_hex_len;
extern guint8 firmware_fw_hex[];
extern unsigned int firmware_fw_hex_len;
extern guint8 firmware_tpfw_bin[];
extern unsigned int firmware_tpfw_bin_len;

static gint
read_block_start(gint length)
{
	guint8 buf[6];

	printf(">>> USB: read_block_start (length=%d)\n", length);

	buf[0] = 0x05;//report id
	buf[1] = 0x52;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = length & 0xFF;
	buf[5] = (length >> 8) & 0xFF;

	return libusb_control_transfer(devh, 0x21, 0x09, 0x0305, 0,
		buf, sizeof(buf), 100);
}

static gint read_block(guint8 *data, gint offset, gint length)
{
	guint8 buf[length + 2];

	printf(">>> USB: read_block (offset=%d, length=%d)\n", offset, length);

	buf[0] = 0x06;//report id
	buf[1] = 0x72;

	gint rc = libusb_control_transfer(devh, 0xa1, 0x01, 0x0306, 0,
		buf, sizeof(buf), 2000);

	if (rc < 0) {
		return rc;
	}

	memcpy(&data[offset], &buf[2], length);

	return 0;
}

gint read_bulk(guint8 *data, gint length)
{
	gint block_size = 2048;
	gint rc;

	rc = read_block_start(length);
	if (rc < 0) {
		return rc;
	}

	for (gint offset = 0; offset < length; offset += block_size) {
		rc = read_block(data, offset, block_size);
		if (rc < 0) {
			return rc;
		}

		usleep(10000);
	}

	return 0;
}

static gint write_block_start(guint16 length)
{
	guint8 buf[6];

	printf(">>> USB: write_block_start (length=%d)\n", length);

	buf[0] = 0x05;//report id
	buf[1] = 0x57;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = length & 0xFF;
	buf[5] = (length >> 8) & 0xFF;

	return libusb_control_transfer(devh, 0x21, 0x09, 0x0305, 0,
		buf, sizeof(buf), 100);
}

static gint write_block(guint8 *data, gint offset, gint length)
{
	guint8 buf[length + 2];

	printf(">>> USB: write_block (offset=%d, length=%d)\n", offset, length);

	buf[0] = 0x06;//report id
	buf[1] = 0x77;

	memcpy(&buf[2], &data[offset], length);

	return libusb_control_transfer(devh, 0x21, 0x09, 0x0306, 0,
		buf, sizeof(buf), 2000);
}

gint
write_bulk(guint8 *data, gint length)
{
	gint block_size = 2048;
	gint rc;

	rc = write_block_start(length);
	if (rc < 0) {
		return rc;
	}

	// HACK: overwrite first byte (as in original sources)
	guint8 first_byte = data[0];
	data[0] = 0;

	for (gint offset = 0; offset < length; offset += block_size) {
		rc = write_block(data, offset, block_size);
		if (rc < 0) {
			return rc;
		}

		usleep(10000);
	}

	data[0] = first_byte;

	// retry write of first block
	rc = write_block_start(length);
	if (rc < 0) {
		return rc;
	}

	rc = write_block(data, 0, block_size);
	if (rc < 0) {
		return rc;
	}

	return 0;
}


static gint read_hexdata(const guint8 *data, gint data_length, guint8 output[MAX_BINLEN])
{
	guint8 pbuffer[MAX_BINLEN];
	const char *endstr = ":00000001FF\n";
	char strbuf[256];
	gint max_address = 0;

	gint data_offset = 0;

	while (data_offset < data_length) {
		// read line
		gint strbuf_idx = 0;
		while (strbuf_idx < sizeof(strbuf)-1) {
			strbuf[strbuf_idx++] = data[data_offset++];
			if (strbuf[strbuf_idx-1] == '\n') {
				break;
			}
		}
		strbuf[strbuf_idx] = 0;

		if (strcmp(endstr, strbuf) == 0) {
			break;
		}

		char len_str[3];
		len_str[0] = strbuf[1];
		len_str[1] = strbuf[2];
		len_str[2] = 0;
		gint len = strtol(len_str, NULL, 16);

		char addr_str[5];
		addr_str[0] = strbuf[3];
		addr_str[1] = strbuf[4];
		addr_str[2] = strbuf[5];
		addr_str[3] = strbuf[6];
		addr_str[4] = 0;
		gint addr = strtol(addr_str, NULL, 16);

		char val_str[3];
		val_str[2] = 0;
		for (gint i = 0; i < len; i++) {
			val_str[0] = strbuf[2 * i + 9];
			val_str[1] = strbuf[2 * i + 1 + 9];
			gint val = strtol(val_str, NULL, 16);
			if (addr >= MAX_BINLEN) {
				break;
			}

			pbuffer[addr++] = val;

			if (addr > max_address) {
				max_address = addr;
			}
		}
	}

	for (gint i = 0; i < max_address; i++)
	{
		output[i] = pbuffer[i];
	}

	if (output[1] == 0x38 && output[2] == 0x00)
	{
		printf(">>> Fix hex file\n");
		output[0] = pbuffer[0x37FB];
		output[1] = pbuffer[0x37FC];
		output[2] = pbuffer[0x37FD];

		output[0x37FB] = 0x00;
		output[0x37FC] = 0x00;
		output[0x37FD] = 0x00;
	}

	return max_address;
}

gint convert_hex_data(const guint8 *data, gint data_length, const char *output_filename)
{
	guint8 hex_file[MAX_BINLEN];
	gint hex_file_length;

	hex_file_length = read_hexdata(data, data_length, hex_file);
	if (hex_file_length <= 0) {
		printf(">>> Failed to read: %d\n", data_length);
		return -1;
	}

	printf("[*] Writing %s\n", output_filename);
	FILE *fp = fopen(output_filename, "wb");
	if (!fp) {
		printf(">>> Failed to write: %s\n", output_filename);
		return -1;
	}

	gint rc = fwrite(hex_file, 1, hex_file_length, fp);
	if (rc != hex_file_length) {
		printf(">>> Failed to write all data: %d != %d\n", rc, hex_file_length);
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}


void close_usb()
{
	if (devh) {
		printf(">>> release interface\r\n");
		libusb_release_interface(devh, devintf);
		libusb_close(devh);
		libusb_exit(ctx);
		devh = NULL;
	}
}

gint open_usb(gint vid, guint16 pid, guint16 indf)
{
	gint rc;

	rc = libusb_init(&ctx);
	if(rc < 0)
		return rc;
	
	devintf = indf;

	printf(">>> Trying to open VID:%04x PID:%04x...\n", vid, pid);
	devh = libusb_open_device_with_vid_pid(ctx, vid, pid);
	if(devh == NULL) {
		printf(">>> Device not found\n");
		return -1;
	}

	rc = libusb_kernel_driver_active(devh, indf);
	if (rc > 0) {
		printf(">>> Kernel Driver Active\n");
		rc = libusb_detach_kernel_driver(devh, indf);
		if (rc < 0) {
			printf(">>> libusb_detach_kernel_driver: %d\n", rc);
			goto finish;
		}
	}

	rc = libusb_claim_interface(devh, indf);
	if(rc < 0) {
		printf(">>> libusb_claim_interface: %d\n", rc);
		goto finish;
	}

finish:
	if (rc < 0) {
		close_usb();
	}
	return rc;
}

gint open_user_mode()
{
	gint rc = open_usb(0x258a, 0x001e, 1);
	if (rc < 0) {
		rc = open_usb(0x258a, 0x001f, 1);
	}
	if (rc < 0) {
		rc = open_usb(0x258a, 0x000d, 1);
	}

	return rc;
}

gint open_touchpad_mode()
{
	gint rc = open_usb(0x258a, 0x001f, 1);
	if (rc < 0) {
		rc = open_usb(0x258a, 0x000d, 1);
	}

	return rc;
}

gint open_boot_mode()
{
	return open_usb(0x0603, 0x1020, 0);
}

gint switch_to_boot_mode()
{
	gint rc, try;

	printf("[*] Opening in user mode...\n");
	for (try = 0; try < 3; try++) {
		rc = open_user_mode();
		if (rc >= 0) {
			break;
		}
		usleep(500*1000);
	}

	if (try == 3) {
		printf(">>> Failed to open in user mode\n");
		goto finish;
	}

	printf("[*] Sending command to switch to boot mode...\n");

	guint8 dataOut[6] = {
		0x5, 0x75
	};
	rc = libusb_control_transfer(devh, 0x21, 0x09, 0x0305, 1,
		dataOut, sizeof(dataOut), 1000);
	if (rc < 0) {
		printf("failed to send switch command\n");
		goto finish;
	}

	printf("[*] Command send\n");

finish:
	close_usb();
	return rc;
}

gint reset_device()
{
	gint rc;

	guint8 data[6] = {
		0x05, 0x55, 0x55, 0x55, 0x55, 0x55
	};

	rc = libusb_control_transfer(devh, 0x21, 0x09, 0x0305, 0, data, sizeof(data), 100);
	if (rc < 0) {
		return rc;
	}

	rc = libusb_reset_device(devh);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

gint write_kb_fw(const guint8 *data, gint data_length)
{
	guint8 hex_file[MAX_BINLEN];
	guint8 read_hex_file[MAX_BINLEN];
	gint hex_file_length;
	gint rc;
	gint try;

	hex_file_length = read_hexdata(data, data_length, hex_file);
	if (hex_file_length <= 0) {
		printf(">>> Failed to read: %d\n", data_length);
		return -1;
	}

	switch_to_boot_mode();

	printf("[*] Opening in boot mode\n");
	for (try = 0; try < 20; try++) {
		rc = open_boot_mode();
		if (rc >= 0) {
			break;
		}
		usleep(100*1000);
	}

	if (try == 20) {
		printf(">>> Failed to open in boot mode\n");
		goto finish;
	}

	guint8 reportData[6] = {
		0x5, 0x45, 0x45, 0x45, 0x45, 0x45
	};

	// flash erase
	printf("[*] Erasing flash\n");
	rc = libusb_control_transfer(devh, 0x21, 0x09, 0x0305, 0,
		reportData, sizeof(reportData), 100);
	if (rc < 0) {
		printf("failed to erase flash\n");
		goto finish;
	}

	sleep(2);

	printf("[*] Writing firmware...\n");
	// write FW
	for (try = 0; try < 5; try++) {
		rc = write_bulk(hex_file, hex_file_length);
		if (rc == 0) {
			break;
		}
	}

	if (try == 5) {
		printf("too many tries\n");
		rc = -1;
		goto finish;
	}

	printf("[*] Reading back firmware...\n");
	// read FW
	for (try = 0; try < 5; try++) {
		rc = read_bulk(read_hex_file, hex_file_length);
		if (rc == 0) {
			break;
		}
	}

	if (try == 5) {
		printf("too many tries\n");
		rc = -1;
		goto finish;
	}

	printf("[*] Comparing firmwares...\n");
	if (memcmp(hex_file, read_hex_file, 0x37fb)) {
		printf("FATAL ERROR FW does differ\n");
		for (gint i = 0; i < hex_file_length; i++) {
			if (hex_file[i] == read_hex_file[i]) {
				continue;
			}
			printf(">>> 0x%04x] %02x != %02x\n", i, hex_file[i], read_hex_file[i]);
		}
		rc = -1;
		goto finish;
	}

	printf("[*] Reseting device?\n");
	reset_device();

	printf("[*] Finished succesfully!\n");
finish:
	close_usb();
	return rc;
}

#define SHORTREPOID			0x05
#define LONGREPOID			0x06
#define SHORTDEVLENGHT		6
#define LONGDEVLENGHT		1040


#define STATUSCMD				  0xA1


#define CHECKCHECKSUM             0xF0
#define ENTERBOOTLOADER           0xF1
#define ICERASE                   0xF2
#define PROGRAM                   0xF3
#define VERIFY1KDATA              0xF4
#define VERIFY_CHECKSUM           0xF5
#define PROGRAMPASS               0xF6
#define ENDPROGRAM                0xF7
#define I2C_PASS                  0xFA
#define I2C_FAIL                  0xFB
#define I2C_WAIT                  0xFC
#define I2C_TIMEOUT               0xFD

#define CHECKCHECKSUM_PASS              0xE0
#define ENTERBOOTLOADER_PASS            0xE1
#define ICERASE_PASS                    0xE2
#define PROGRAM_PASS                    0xE3
#define VERIFY1KDATA_PASS               0xE4
#define VERIFY_CHECKSUM_PASS            0xE5
#define PROGRAMPASS_PASS                0xE6
#define ENDPROGRAM_PASS                 0xE7

#define CHECKCHECKSUM_FAIL              0xD0
#define ENTERBOOTLOADER_FAIL            0xD1
#define ICERASE_FAIL                    0xD2
#define PROGRAM_FAIL                    0xD3
#define VERIFY1KDATA_FAIL               0xD4
#define VERIFY_CHECKSUM_FAIL            0xD5
#define PROGRAMPASS_FAIL                0xD6
#define ENDPROGRAM_FAIL                 0xD7

#define PROGMODE				  0x0b

static gint touchpad_verify(gint type, gint pass, gint sendcmd)
{
	if (sendcmd) {
		guint8 data[6] = {
			SHORTREPOID, STATUSCMD, type
		};

		gint rc = libusb_control_transfer(devh, 0x21, 0x09, 0x0305, 1, data, sizeof(data), 1000);
		if(rc < 0) {
			return -1;
		}
	}

	if (1) { // receive command
		guint8 data[6] = {};

		gint rc = libusb_control_transfer(devh, 0xa1, 0x01, 0x0305, 1, data, sizeof(data), 2000);
		if(rc < 0) {
			return -1;
		}

		if (data[0] != SHORTREPOID || data[1] != pass) {
			printf(">>> Verify mismatch: type=%02x, pass=%02x, received=%02x\n", type, pass, data[1]);
			return -1;
		}
	}

	return 0;
}

gint try_touchpad_verify(gint type, gint pass, gint sendcmd)
{
	gint try;

	for (try = 0; try < 100; try++) {
		usleep(50*1000);

		gint rc = touchpad_verify(type, pass, sendcmd);
		if (rc == 0) {
			break;
		}
	}

	if (try == 100) {
		printf(">>> Touchpad verify (type=%d, pass=%d) data failed\n", type, pass);
		return -1;
	}

	return 0;
}

gint write_tp_fw(const guint8 *fw, gint fw_length)
{
	gint block_size = 1024;
	gint try;
	gint rc;

	if (fw_length < 24576) {
		printf("[*] Touchpad firmware needs to be %d, and is %d\n",
			fw_length, 24576);
		return -1;
	}

	// cap to 24k
	fw_length = 24 * 1024;

	printf("[*] Opening in touchpad mode\n");
	for (try = 0; try < 3; try++) {
		rc = open_touchpad_mode();
		if (rc >= 0) {
			break;
		}
		usleep(500*1000);
	}

	if (try == 20) {
		printf(">>> Failed to open in touchpad mode\n");
		goto finish;
	}

	rc = try_touchpad_verify(ICERASE, ICERASE_PASS, 0);
	if (rc < 0) {
		printf(">>> Touchpad erase failed\n");
		goto finish;
	}

	usleep(10000);

	for(gint offset = 0; offset < fw_length; offset += block_size)
	{
		guint8 data[16 + block_size];
		guint8 *ptr = data;

		*ptr++ = 0x06;
		*ptr++ = 0xD0;
		*ptr++ = 0xCC;
		*ptr++ = 0xCC;
		*ptr++ = offset & 0xFF;
		*ptr++ = (offset >> 8) & 0xFF;
		*ptr++ = 0xCC;
		*ptr++ = 0xCC;

		memset(ptr, 0, block_size);
		memcpy(ptr, &fw[offset], MIN(block_size, fw_length-offset));
		ptr += block_size;
		
		*ptr++ = 0xEE;
		*ptr++ = 0xD2;
		*ptr++ = 0xCC;
		*ptr++ = 0xCC;
		*ptr++ = 0xCC;
		*ptr++ = 0xCC;
		*ptr++ = 0xCC;
		*ptr++ = 0xCC;

		printf(">>> Writing offset:%d length:%d...\n", offset, block_size);

		rc = libusb_control_transfer(devh, 0x21, 0x09, 0x0306, 1, data, sizeof(data), 1000);
		if (rc < 0) {
			printf(">>> Write failed\n");
			goto finish;
		}
		
		usleep(150*1000);

		printf(">>> Verifying '1k-data'...\n");

		rc = try_touchpad_verify(VERIFY1KDATA, VERIFY1KDATA_PASS, 1);
		if (rc < 0) {
			printf(">>> Touchpad verify data failed\n");
			goto finish;
		}
	}

	usleep(50*1000);
	
	printf("[*] Verifying 'end-program'...\n");

	rc = try_touchpad_verify(ENDPROGRAM, ENDPROGRAM_PASS, 1);
	if (rc < 0) {
		printf(">>> Touchpad end program verify\n");
		goto finish;
	}

	usleep(50*1000);

	printf("[*] Verifying 'checksum'...\n");

	rc = try_touchpad_verify(VERIFY_CHECKSUM, VERIFY_CHECKSUM_PASS, 1);
	if (rc < 0) {
		printf(">>> Touchpad end program verify\n");
		goto finish;
	}

	usleep(50*1000);

	printf("[*] Verifying 'program'...\n");
	
	rc = try_touchpad_verify(PROGRAMPASS, 0, 1);
	if (rc < 0) {
		printf(">>> Touchpad end program verify\n");
		goto finish;
	}

	printf("[*] Finished succesfully!\n");

finish:
	close_usb();
	return rc;
}

static gint usage(const char *cmd)
{
	printf("usage: %s [step-1|step-2]\n", cmd);
	return -1;
}

static gint convert()
{
	gint rc;
		
	rc = convert_hex_data(
		firmware_fw_tp_update_hex, firmware_fw_tp_update_hex_len,
		"fw_tp_update.bin");
	if (rc < 0) {
		return rc;
	}

	rc = convert_hex_data(
		firmware_fw_hex, firmware_fw_hex_len,
		"fw.bin");
	if (rc < 0) {
		return rc;
	}

	return 0;
}

static gint flash_tp()
{
	gint rc;

	rc = write_tp_fw(firmware_tpfw_bin, firmware_tpfw_bin_len);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

static gint flash_tp_update()
{
	gint rc;

	rc = write_kb_fw(firmware_fw_tp_update_hex, firmware_fw_tp_update_hex_len);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

static gint
flash_kb()
{
	gint rc;

	rc = write_kb_fw(firmware_fw_hex, firmware_fw_hex_len);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

static gint
step_1()
{
	gint rc;

	printf("Running STEP-1...\n");

	printf("[*] Flashing keyboard updater firmware...\n");
	rc = flash_tp_update();
	if (rc < 0) {
		return rc;
	}
	
	printf("[*] Please reboot now, and run `step-2`.\n");
	
	return 0;
}

static gint
step_2()
{
	gint rc;

	printf("Running STEP-2...\n");

	printf("[*] Flashing touchpad firmware...\n");
	rc = flash_tp();
	if (rc < 0) {
		return rc;
	}

	printf("[*] Flashing keyboard firmware...\n");
	rc = flash_kb();
	if (rc < 0) {
		return rc;
	}

	printf("[*] All done! Your keyboard and touchpad should be updated.\n");

	return 0;
}

#endif
