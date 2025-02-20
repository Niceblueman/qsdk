#
# Base-files consolidation for IPQ chipsets
#

BASEFILES_DIR:=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))

EXTRA_FILES:= \
	/etc/config/network \
	/sbin/firstboot \
	/cron \
	/sbin/led.sh \
	/sbin/sysdebug \
	/etc/rc.button/power \
	/powerctl \
	/lib/functions/leds.sh \
	/lib/debug/system \
	/etc/init.d/led \
	/etc/init.d/powerctl \
	/etc/rc.button/rfkill \
	/etc/rc.button/power \
	/lib/functions/uci-defaults-new.sh \
	/lib/functions/leds.sh \
	/bin/board_detect

define base-files_install_append
ifneq (, $(findstring ipq, $(CONFIG_TARGET_BOARD)))
	$(CP) $(BASEFILES_DIR)/files/* $(1)/
endif
ifeq (, $(findstring ipq95xx, $(CONFIG_TARGET_BOARD)))
	rm -rf $(1)/sbin/gps_start
endif
endef

Package/base-files/install += $(newline)$(base-files_install_append)
