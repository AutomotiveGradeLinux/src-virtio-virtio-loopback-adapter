/*
 * Based on vhost-user-rng of QEMU project
 *
 * Copyright (c) 2021 Mathieu Poirier <mathieu.poirier@linaro.org>
 *
 * Copyright (c) 2022-2023 Virtual Open Systems SAS.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/param.h>

/* Project header files */
#include "vhost_loopback.h"
#include "vhost_user_rng.h"

#ifdef DEBUG
#define DBG(...) printf("vhost-user-rng: " __VA_ARGS__)
#else
#define DBG(...)
#endif /* DEBUG */

static const int user_feature_bits[] = {
    VIRTIO_F_VERSION_1,
    VIRTIO_F_NOTIFY_ON_EMPTY,
    VIRTIO_RING_F_INDIRECT_DESC,
    VIRTIO_RING_F_EVENT_IDX,
    VIRTIO_F_RING_RESET,
    VHOST_INVALID_FEATURE_BIT
};

static void vu_rng_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    (void)vdev;
    (void)vq;
    /*
     * Not normally called; it's the daemon that handles the queue;
     * however virtio's cleanup path can call this.
     */
    DBG("vu_rng_handle_output\n");
}

void vhost_user_rng_init(VirtIODevice *vdev);

void vhost_user_rng_realize(struct adapter_dev *adev)
{
    VirtIODevice *vdev = &adev->virtio_dev;
    struct vhost_dev *vhdev = &adev->vdev;
    VirtIOMMIOProxy *proxy = &adev->proxy;

    /* Initialize proxy */
    proxy->legacy = 1;

    /* Initiliaze virtio_dev data structures */
    virtio_dev_init(vdev, "virtio-rng", 4, 0);
    vdev->user_feature_bits = user_feature_bits;

    /* This needs to be change to vhost-user-rng init */
    vhost_user_rng_init(vdev);

    vdev->vq = virtio_add_queue(vdev, 4, vu_rng_handle_output);
    vdev->host_features = 0x39000000;

    /* Virtqueues conf */
    vhdev->nvqs = 1;
    vhdev->vqs = (struct vhost_virtqueue *)malloc(vhdev->nvqs *
                                            sizeof(struct vhost_virtqueue));

    /* Initiale vhost-user communication */
    vhost_dev_init(vhdev);

    /* Write the final features */
    vdev->host_features = vhdev->features;
    DBG("final features: 0x%lx\n", vhdev->features);
}

static void vhost_user_dev_class_init_extra(VirtIODevice *vdev)
{
    vdev->vdev_class->realize = vhost_user_rng_realize;
}

void vhost_user_rng_init(VirtIODevice *vdev)
{
    VHostUserRNG *vhrng = (VHostUserRNG *)malloc(sizeof(VHostUserRNG));
    vdev->vhrng = vhrng;
    vdev->nvqs = (int *)&vdev->vhdev->nvqs;
    vhrng->parent = vdev;
    vhrng->req_vq = vdev->vq;
    vhrng->vhost_dev = vdev->vhdev;

    vhost_user_dev_class_init(vdev);
    vhost_user_dev_class_init_extra(vdev);
    virtio_loopback_bus_init(vdev->vbus);
}
