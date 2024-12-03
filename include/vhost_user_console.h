/*
 *
 * Copyright (c) 2023-2024 Virtual Open Systems SAS.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 *
 */

#ifndef VHOST_USER_CONSOLE
#define VHOST_USER_CONSOLE

#include "virtio_loopback.h"
#include "vhost_loopback.h"
#include "vhost_user_loopback.h"

/* Feature bits */
#define VIRTIO_CONSOLE_F_SIZE 0        /* Does host provide console size? */
#define VIRTIO_CONSOLE_F_MULTIPORT 1   /* Does host provide multiple ports? */
#define VIRTIO_CONSOLE_F_EMERG_WRITE 2 /* Does host support emergency write? */

struct virtio_console_config {
    /* colums of the screens */
    __virtio16 cols;
    /* rows of the screens */
    __virtio16 rows;
    /* max. number of ports this device can hold */
    __virtio32 max_nr_ports;
    /* emergency write register */
    __virtio32 emerg_wr;
};

typedef struct VHostUserConsole {
    VirtIODevice *parent;
    struct vhost_virtqueue *vhost_vqs;
    VirtQueue **virtqs;
    uint16_t num_queues;
    uint32_t queue_size;
    struct virtio_console_config config;
    struct vhost_dev *vhost_dev;
} VHostUserConsole;

void vhost_user_console_realize(struct adapter_dev *adev);

#endif /* VHOST_USER_CONSOLE */
