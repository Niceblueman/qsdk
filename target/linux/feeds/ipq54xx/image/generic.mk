
define Device/qcom_maxx
	$(call Device/MultiDTBFitImage)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := AP-MAXX
	DEVICE_VARIANT :=
	BOARD_NAME := ap-maxx
	SOC := devsoc
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += qcom_maxx

define Device/emulation
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL :=
	DEVICE_VARIANT :=
	BOARD_NAME :=
	BUILD_DTS_devsoc-emulation := 1
	SOC := devsoc
	KERNEL_INSTALL := 1
	KERNEL_SIZE := 6096k
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += emulation
