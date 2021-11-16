include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=clink-ko
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/clink-ko
	CATEGORY:=CAMEO Proprietary Software
	DEPENDS+= +libc +PACKAGE_kmod-mt7628-ko:kmod-mt7628-ko
	TITLE:=Kernel Module for Communication by Netlink
	MAINTAINER:=JoE HuAnG
	FILES:=$(PKG_BUILD_DIR)/clink.ko
	AUTOLOAD:=$(call AutoLoad,99,clink)
endef

define KernelPackage/clink-ko/config
	source "$(SOURCE)/Config.in"
endef

EXTRA_KCONFIG:= \
	CONFIG_CAMEO_NETLINK=m

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	+$(MAKE) $(PKG_JOBS) -C "$(LINUX_DIR)" \
		ARCH="$(LINUX_KARCH)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		SUBDIRS="$(PKG_BUILD_DIR)" \
		CFLAGS="$(CFLAGS)"  \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		$(EXTRA_KCONFIG) \
		modules
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/*.h $(1)/usr/include
endef

ifeq ($(CONFIG_UPDATE_WIFI_LED),y)
	EXTRA_CFLAGS+=-DCONFIG_UPDATE_WIFI_LED
endif

ifeq ($(CONFIG_UPDATE_RSSI_LED),y)
	EXTRA_CFLAGS+=-DCONFIG_UPDATE_RSSI_LED
endif

ifeq ($(CONFIG_HAVE_PHY_ETHERNET),y)
	EXTRA_CFLAGS+=-DCONFIG_HAVE_PHY_ETHERNET
endif

ifeq ($(CONFIG_POWER_SAVING),y)
	EXTRA_CFLAGS+=-DCONFIG_POWER_SAVING
endif

ifeq ($(CONFIG_UPDATE_WAN_STATUS),y)
	EXTRA_CFLAGS+=-DCONFIG_UPDATE_WAN_STATUS
endif

$(eval $(call KernelPackage,clink-ko))

