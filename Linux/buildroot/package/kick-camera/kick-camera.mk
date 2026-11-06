# SPDX-License-Identifier: AGPL-3.0-or-later
################################################################################
#
# kick-camera
#
################################################################################

KICK_CAMERA_VERSION = 1.0.0
# This repository holds Buildroot at Linux/buildroot and the app at Kick/, so the source is a sibling directory.
# The whole Kick/ tree is synced into the build directory (about 14 MB, it never reaches the image), and cmake
# builds only the service subdirectory. That keeps Kick/LICENSE available for the legal-info output.
KICK_CAMERA_SITE = $(TOPDIR)/../../Kick
KICK_CAMERA_SITE_METHOD = local
KICK_CAMERA_SUBDIR = services/kick_camera
KICK_CAMERA_LICENSE = AGPL-3.0-or-later
KICK_CAMERA_LICENSE_FILES = LICENSE

# The tests fetch googletest from the network, which a Buildroot build must not do.
KICK_CAMERA_CONF_OPTS = -DKICK_CAMERA_BUILD_TESTS=OFF

define KICK_CAMERA_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 0644 $(@D)/$(KICK_CAMERA_SUBDIR)/systemd/kick-camera.service \
		$(TARGET_DIR)/usr/lib/systemd/system/kick-camera.service
	$(INSTALL) -d $(TARGET_DIR)/etc/systemd/system/multi-user.target.wants
	ln -sf ../../../../usr/lib/systemd/system/kick-camera.service \
		$(TARGET_DIR)/etc/systemd/system/multi-user.target.wants/kick-camera.service
endef

define KICK_CAMERA_INSTALL_CONFIG
	$(INSTALL) -D -m 0644 $(@D)/$(KICK_CAMERA_SUBDIR)/systemd/kick-camera.conf \
		$(TARGET_DIR)/etc/kick-camera.conf
endef
KICK_CAMERA_POST_INSTALL_TARGET_HOOKS += KICK_CAMERA_INSTALL_CONFIG

$(eval $(cmake-package))
