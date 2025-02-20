#
# Copyright (C) 2007-2011 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=wlan-iw
PKG_VERSION:=5.16
PKG_RELEASE:=1
PKG_LICENSE:=ISC

PKG_SOURCE:=iw-$(PKG_VERSION).tar.xz
PKG_SOURCE_URL:=https://www.kernel.org/pub/software/network/iw
PKG_MD5SUM:=782a3460da2854bd2e5b8f96845a62f8

PKG_MAINTAINER:=Felix Fietkau <nbd@openwrt.org>
PKG_LICENSE:=ISC

PKG_BUILD_DIR=$(BUILD_DIR)/iw-$(PKG_VERSION)
PATCH_DIR=$(TOPDIR)/src/ipq/wlan-iw/package/network/utils/iw/patches

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=net
  CATEGORY:=Network
  TITLE:=cfg80211 interface configuration utility
  URL:=http://wireless.kernel.org/en/users/Documentation/iw
  DEPENDS:= +libnl-tiny
endef

define Package/$(PKG_NAME)-full
  $(Package/iw)
  TITLE += (full version)
  VARIANT:=full
  PROVIDES:=$(PKG_NAME)
endef

define Build/Configure
	echo "const char iw_version[] = \"$(PKG_VERSION)\";" > $(PKG_BUILD_DIR)/version.c
	rm -f $(PKG_BUILD_DIR)/version.sh
	touch $(PKG_BUILD_DIR)/version.sh
	chmod +x $(PKG_BUILD_DIR)/version.sh
endef

TARGET_CFLAGS += -fpie -Wall -Werror
TARGET_LDFLAGS += -pie

TARGET_CPPFLAGS:= \
	-I$(STAGING_DIR)/usr/include/libnl-tiny\
	$(TARGET_CPPFLAGS) \
	-DCONFIG_LIBNL20 \
	-D_GNU_SOURCE

ifeq ($(BUILD_VARIANT),full)
  TARGET_CPPFLAGS += -DIW_FULL
  MAKE_FLAGS += IW_FULL=1
endif

MAKE_FLAGS += \
	CFLAGS="$(TARGET_CPPFLAGS) $(TARGET_CFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="$(TARGET_LDFLAGS) -Wl,--gc-sections" \
	NL1FOUND="" NL2FOUND=Y \
	NLLIBNAME="libnl-tiny" \
	LIBS="-lm -lnl-tiny" \
	V=1

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/iw $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
