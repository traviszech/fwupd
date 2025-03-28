/*
 * Copyright 2025 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "FuEfiX509Device"

#include "config.h"

#include "fu-efi-x509-device.h"
#include "fu-version-common.h"

/**
 * FuEfiX509Device
 *
 * See also: #FuUdevDevice
 */

typedef struct {
	FuEfiX509Signature *sig;
} FuEfiX509DevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(FuEfiX509Device, fu_efi_x509_device, FU_TYPE_DEVICE)

#define GET_PRIVATE(o) (fu_efi_x509_device_get_instance_private(o))

static gboolean
fu_efi_x509_device_probe(FuDevice *device, GError **error)
{
	FuEfiX509Device *self = FU_EFI_X509_DEVICE(device);
	FuEfiX509DevicePrivate *priv = GET_PRIVATE(self);

	/* sanity check */
	if (priv->sig == NULL) {
		g_set_error_literal(error, FWUPD_ERROR, FWUPD_ERROR_NOT_SUPPORTED, "no sig");
		return FALSE;
	}

	/* these have to exist */
	fu_device_add_instance_strsafe(device,
				       "VENDOR",
				       fu_efi_x509_signature_get_subject_vendor(priv->sig));
	fu_device_add_instance_strsafe(device,
				       "NAME",
				       fu_efi_x509_signature_get_subject_name(priv->sig));
	if (!fu_device_build_instance_id(device, error, "UEFI", "VENDOR", "NAME", NULL))
		return FALSE;
	fu_device_set_name(device, fu_efi_x509_signature_get_subject_name(priv->sig));
	fu_device_set_vendor(device, fu_efi_x509_signature_get_subject_vendor(priv->sig));
	fu_device_set_version_raw(device, fu_firmware_get_version_raw(FU_FIRMWARE(priv->sig)));
	fu_device_set_logical_id(device, fu_firmware_get_id(FU_FIRMWARE(priv->sig)));
	fu_device_build_vendor_id(device,
				  "UEFI",
				  fu_efi_x509_signature_get_subject_vendor(priv->sig));

	/* success */
	fu_device_add_instance_strup(device, "CRT", fu_firmware_get_id(FU_FIRMWARE(priv->sig)));
	return fu_device_build_instance_id(device, error, "UEFI", "CRT", NULL);
}

static gchar *
fu_efi_x509_device_convert_version(FuDevice *device, guint64 version_raw)
{
	return fu_version_from_uint64(version_raw, fu_device_get_version_format(device));
}

static gboolean
fu_efi_x509_device_write_firmware(FuDevice *device,
				  FuFirmware *firmware,
				  FuProgress *progress,
				  FwupdInstallFlags flags,
				  GError **error)
{
	FuDeviceClass *device_class;
	FuDevice *proxy;

	/* process by the parent */
	proxy = fu_device_get_proxy(device);
	if (proxy == NULL) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_NOT_SUPPORTED,
			    "no proxy device assigned");
		return FALSE;
	}
	device_class = FU_DEVICE_GET_CLASS(proxy);
	return device_class->write_firmware(proxy, firmware, progress, flags, error);
}

static void
fu_efi_x509_device_set_progress(FuDevice *self, FuProgress *progress)
{
	fu_progress_set_id(progress, G_STRLOC);
	fu_progress_add_flag(progress, FU_PROGRESS_FLAG_GUESSED);
	fu_progress_add_step(progress, FWUPD_STATUS_DECOMPRESSING, 0, "prepare-fw");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 0, "detach");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_WRITE, 100, "write");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 0, "attach");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_BUSY, 0, "reload");
}

static void
fu_efi_x509_device_init(FuEfiX509Device *self)
{
	fu_device_set_version_format(FU_DEVICE(self), FWUPD_VERSION_FORMAT_NUMBER);
	fu_device_add_protocol(FU_DEVICE(self), "org.uefi.dbx2");
	fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_UPDATABLE);
	fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_SIGNED_PAYLOAD);
	fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_INTERNAL);
	fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_CAN_EMULATION_TAG);
	fu_device_add_icon(FU_DEVICE(self), "application-certificate");
}

static void
fu_efi_x509_device_finalize(GObject *obj)
{
	FuEfiX509Device *self = FU_EFI_X509_DEVICE(obj);
	FuEfiX509DevicePrivate *priv = GET_PRIVATE(self);
	if (priv->sig != NULL)
		g_object_unref(priv->sig);
	G_OBJECT_CLASS(fu_efi_x509_device_parent_class)->finalize(obj);
}

static void
fu_efi_x509_device_class_init(FuEfiX509DeviceClass *klass)
{
	FuDeviceClass *device_class = FU_DEVICE_CLASS(klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = fu_efi_x509_device_finalize;
	device_class->probe = fu_efi_x509_device_probe;
	device_class->convert_version = fu_efi_x509_device_convert_version;
	device_class->write_firmware = fu_efi_x509_device_write_firmware;
	device_class->set_progress = fu_efi_x509_device_set_progress;
}

/**
 * fu_efi_x509_device_new:
 * @ctx: (not nullable): a #FuContext
 * @sig: (not nullable): a #FuEfiX509Signature
 *
 * Creates a new X.509 EFI device.
 *
 * Returns: (transfer full): a #FuEfiX509Device
 *
 * Since: 2.0.8
 **/
FuEfiX509Device *
fu_efi_x509_device_new(FuContext *ctx, FuEfiX509Signature *sig)
{
	g_autoptr(FuEfiX509Device) self =
	    g_object_new(FU_TYPE_EFI_X509_DEVICE, "context", ctx, NULL);
	FuEfiX509DevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(FU_IS_EFI_X509_SIGNATURE(sig), NULL);
	priv->sig = g_object_ref(sig);
	return g_steal_pointer(&self);
}
