
ARCH:=arm
SUBTARGET:=ipq95xx_32
BOARDNAME:=QTI IPQ95xx(32bit) based boards
CPU_TYPE:=cortex-a7

define Target/Description
	Build firmware image for IPQ95xx SoC devices.
endef

DEFAULT_PACKAGES += \
	uboot-2016-ipq9574 uboot-2016-ipq9574-debug uboot-ipq9574-mmc32 \
	uboot-ipq9574-norplusmmc32 uboot-ipq9574-norplusnand32 \
	uboot-ipq9574-nand32 fwupgrade-tools \
	sysupgrade-helper
