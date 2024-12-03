# virtio-loopback adapter repository

This repository includes the "virtio_loopback_adapter" application which is part of the Virtio-loopback design. If you want to learn more about how to set up and test the whole virtio-loopback architecture, refer to the [virtio-loopback testing guide](https://gerrit.automotivelinux.org/gerrit/gitweb?p=src/virtio/virtio-loopback-adapter.git;a=blob;f=Documentation/testing_virtio_loopback_design.md;hb=HEAD).

This work carried on by Virtual Open Systems in the [Automotive Grade Linux](https://www.automotivegradelinux.org) community.

## Build the virtio-loopback adapter

In order to build this project the next commands need to be used:
- `make` for x86
- `make ARCH=arm64` for arm64
- `make ARCH=riscv64` for riscv64

**NOTE**: You can also use the parameter "DEBUG=1" in order to enable the debug messages.
