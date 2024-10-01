/*
 * Based on virtio-gpio.h of QEMU project
 *
 * Copyright (c) 2023-2024 Virtual Open Systems SAS.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 */

#ifndef VHOST_USER_GPIO
#define VHOST_USER_GPIO

#include "vhost_loopback.h"
#include "vhost_user_loopback.h"
#include "virtio_loopback.h"

#define VIRTIO_GPIO_F_IRQ 0

struct virtio_gpio_config {
    uint16_t ngpio;
    uint8_t padding[2];
    uint32_t gpio_names_size;
};

struct VHostUserGPIO {
    VirtIODevice *parent;
    struct virtio_gpio_config config;
    struct vhost_virtqueue *vhost_vqs;
    struct vhost_dev *vhost_dev;
    uint16_t num_queues;
    uint32_t queue_size;
};

void vu_gpio_device_realize(struct adapter_dev *adev);

#endif /* VHOST_USER_GPIO */
