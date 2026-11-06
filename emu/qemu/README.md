# Stage 2: QEMU

Boots the real Buildroot rootfs (systemd, Weston, ALSA, `KT_Kick`) in an aarch64 VM, with the same KICK Surface attached to the second UART over TCP. This is the path that exercises the Linux-only code the host build skips: `/dev/ttymxc0` handling, the mount and reboot paths, systemd units, firmware update.

**Status: scaffold only.** Nothing here has been built or booted. `emu/run.sh` (stage 1) is the working path.

Files:

- `KTKick_emu.fragment`: Buildroot overrides (mainline kernel, no U-Boot, ext4 rootfs).
- `build-image.sh`: Docker build of the image.
- `run-qemu.sh`: starts the Surface on TCP 50910 and boots QEMU with virtio-gpu, virtio-sound, virtio-blk.

Known gaps before it boots Kick:

1. Cross-build `Kick` for the Buildroot toolchain and place it at `/usr/bin/KT_Kick` in the overlay. Not scripted.
2. Confirm the Surface's `--tcp` listener accepts QEMU's client connection and that frames survive the extra UART layer.
3. Add a systemd unit to the overlay that symlinks `/dev/ttymxc0` to `/dev/ttyAMA1`.
4. The Weston config in the overlay targets the i.MX DRM display. The virt machine needs virtio-gpu and a 1024x600 output.
5. The rootfs mounts (`data.mount`, `media-kickstorage.mount`) expect eMMC partitions. Point them at a second virtio-blk disk.
6. The CV DAC is I2C. QEMU virt has no I2C device model for it. Either emulate it with a small daemon behind `/dev/i2c-3` (i2c-stub plus a user-space shim) or leave CV unemulated in the VM.
