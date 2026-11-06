# SPDX-License-Identifier: AGPL-3.0-or-later
################################################################################
#
# kick-gesture
#
################################################################################

KICK_GESTURE_VERSION = 1.0.0
# Same layout as kick-camera: the whole Kick/ tree is synced and cmake builds only the service subdirectory. This
# service also builds the camera service's reader library from ../kick_camera, so that sibling must be present.
KICK_GESTURE_SITE = $(TOPDIR)/../../Kick
KICK_GESTURE_SITE_METHOD = local
KICK_GESTURE_SUBDIR = services/kick_gesture
KICK_GESTURE_LICENSE = AGPL-3.0-or-later, MIT (ONNX Runtime), Apache-2.0 (models)
KICK_GESTURE_LICENSE_FILES = LICENSE
KICK_GESTURE_DEPENDENCIES = kick-camera

# These versions and URLs repeat cmake/pins.cmake. tools/check_pins.py checks pins.cmake against models/MODELS.md, and
# kick-gesture.hash holds the same hashes, so a change has to be made in all three places.
KICK_GESTURE_ORT_VERSION = 1.24.4
KICK_GESTURE_ZOO_COMMIT = 48b10fa505aeb51fbd127fc60a5c660040e823b7
KICK_GESTURE_MODELS = \
	palm_detection_mediapipe_2023feb_int8.onnx \
	handpose_estimation_mediapipe_2023feb_int8.onnx

ifeq ($(BR2_aarch64),y)
KICK_GESTURE_ORT_NAME = onnxruntime-linux-aarch64-$(KICK_GESTURE_ORT_VERSION)
else ifeq ($(BR2_x86_64),y)
KICK_GESTURE_ORT_NAME = onnxruntime-linux-x64-$(KICK_GESTURE_ORT_VERSION)
endif

KICK_GESTURE_EXTRA_DOWNLOADS = \
	https://github.com/microsoft/onnxruntime/releases/download/v$(KICK_GESTURE_ORT_VERSION)/$(KICK_GESTURE_ORT_NAME).tgz \
	https://media.githubusercontent.com/media/opencv/opencv_zoo/$(KICK_GESTURE_ZOO_COMMIT)/models/palm_detection_mediapipe/palm_detection_mediapipe_2023feb_int8.onnx \
	https://media.githubusercontent.com/media/opencv/opencv_zoo/$(KICK_GESTURE_ZOO_COMMIT)/models/handpose_estimation_mediapipe/handpose_estimation_mediapipe_2023feb_int8.onnx

# Buildroot has already downloaded and hash-checked the extra files. Unpack them where cmake is told to look, so the
# cmake fetch step stays off and nothing is downloaded during the build.
KICK_GESTURE_DEPS_DIR = $(@D)/.kick-gesture-deps

define KICK_GESTURE_PREPARE_DEPS
	rm -rf $(KICK_GESTURE_DEPS_DIR)
	mkdir -p $(KICK_GESTURE_DEPS_DIR)/ort $(KICK_GESTURE_DEPS_DIR)/models
	$(TAR) -xzf $(KICK_GESTURE_DL_DIR)/$(KICK_GESTURE_ORT_NAME).tgz --strip-components=1 -C $(KICK_GESTURE_DEPS_DIR)/ort
	$(foreach m,$(KICK_GESTURE_MODELS),cp $(KICK_GESTURE_DL_DIR)/$(m) $(KICK_GESTURE_DEPS_DIR)/models/$(m)$(sep))
endef
KICK_GESTURE_PRE_CONFIGURE_HOOKS += KICK_GESTURE_PREPARE_DEPS

# The tests fetch googletest, and the fetch step downloads, neither of which a Buildroot build may do.
KICK_GESTURE_CONF_OPTS = \
	-DKICK_GESTURE_BUILD_TESTS=OFF \
	-DKICK_GESTURE_FETCH=OFF \
	-DKICK_GESTURE_WITH_ONNX=ON \
	-DKICK_GESTURE_ORT_ROOT=$(KICK_GESTURE_DEPS_DIR)/ort \
	-DKICK_GESTURE_MODEL_DIR=$(KICK_GESTURE_DEPS_DIR)/models

# The binary finds the runtime through RPATH $ORIGIN/../lib, which is /usr/lib on the target. Copy the library with its
# symlinks (libonnxruntime.so -> .so.1 -> .so.1.24.4) so the soname the binary asks for exists.
define KICK_GESTURE_INSTALL_RUNTIME
	cp -a $(KICK_GESTURE_DEPS_DIR)/ort/lib/libonnxruntime.so* $(TARGET_DIR)/usr/lib/
	$(INSTALL) -d $(TARGET_DIR)/usr/share/kick-gesture/models
	$(foreach m,$(KICK_GESTURE_MODELS),$(INSTALL) -m 0644 $(KICK_GESTURE_DEPS_DIR)/models/$(m) \
		$(TARGET_DIR)/usr/share/kick-gesture/models/$(m)$(sep))
endef
KICK_GESTURE_POST_INSTALL_TARGET_HOOKS += KICK_GESTURE_INSTALL_RUNTIME

define KICK_GESTURE_INSTALL_CONFIG
	$(INSTALL) -D -m 0644 $(@D)/$(KICK_GESTURE_SUBDIR)/systemd/kick-gesture.conf \
		$(TARGET_DIR)/etc/kick-gesture.conf
	$(INSTALL) -D -m 0644 $(@D)/$(KICK_GESTURE_SUBDIR)/systemd/gesture.json \
		$(TARGET_DIR)/etc/kick-gesture/gesture.json
endef
KICK_GESTURE_POST_INSTALL_TARGET_HOOKS += KICK_GESTURE_INSTALL_CONFIG

define KICK_GESTURE_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 0644 $(@D)/$(KICK_GESTURE_SUBDIR)/systemd/kick-gesture.service \
		$(TARGET_DIR)/usr/lib/systemd/system/kick-gesture.service
	$(INSTALL) -d $(TARGET_DIR)/etc/systemd/system/multi-user.target.wants
	ln -sf ../../../../usr/lib/systemd/system/kick-gesture.service \
		$(TARGET_DIR)/etc/systemd/system/multi-user.target.wants/kick-gesture.service
endef

$(eval $(cmake-package))
