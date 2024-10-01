/*
 * Based on virtio-input.h of QEMU project
 *
 * Copyright (c) 2022-2024 Virtual Open Systems SAS.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 */

#ifndef VHOST_USER_INPUT
#define VHOST_USER_INPUT

#include "vhost_loopback.h"
#include "vhost_user_loopback.h"
#include "virtio_loopback.h"
#include <linux/virtio_input.h>

typedef struct VhostUserInput {
    VirtIODevice *parent;
    struct vhost_dev *vhost_dev;
    struct virtio_input_config        config;
    VirtQueue                         *evt, *sts;
    bool started;
    bool completed;
} VhostUserInput;

void virtio_input_device_realize(struct adapter_dev *adev);

#endif /* VHOST_USER_INPUT */
