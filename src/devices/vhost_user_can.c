/*
 * Virtio CAN Device
 *
 * Based on virtio_can.h of OpenSynergy's virtio-can RFC
 * https://github.com/OpenSynergy/qemu/tree/virtio-can-spec-rfc-v3
 *
 * Copyright (C) 2021-2023 OpenSynergy GmbH
 * Copyright (c) 2023 Virtual Open Systems SAS.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/param.h>

/* Project header files */
#include "vhost_user_can.h"

#ifdef DEBUG
#define DBG(...) printf("vhost-user-can: " __VA_ARGS__)
#else
#define DBG(...)
#endif /* DEBUG */

/***************************** vhost-user-can ******************************/

static const int user_feature_bits[] = {
    VIRTIO_F_VERSION_1,
    VIRTIO_RING_F_INDIRECT_DESC,
    VIRTIO_RING_F_EVENT_IDX,
    VIRTIO_F_RING_RESET,
    VIRTIO_F_NOTIFY_ON_EMPTY,
    VHOST_INVALID_FEATURE_BIT
};

static void vhost_user_can_can_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    /*
     * Not normally called; it's the daemon that handles the queue;
     * however virtio's cleanup path can call this.
     */
    DBG("vhost_user_can_can_handle_output: Not yet implemented!\n");
    (void)vdev;
    (void)vq;
}

static int vhost_user_can_can_config_change(struct vhost_dev *dev)
{
    DBG("vhost_user_can_can_config_change\n");

    int ret = vhost_dev_get_config(dev, (uint8_t *)dev->vdev->config,
                                   sizeof(struct virtio_can_config));
    if (ret < 0) {
        DBG("vhost_user_can_sound_config_change error\n");
        return -1;
    }

    virtio_notify_config(dev->vdev);

    return 0;
}

const VhostDevConfigOps can_config_ops = {
    .vhost_dev_config_notifier = vhost_user_can_can_config_change,
};

static void vhost_user_can_init(VirtIODevice *vdev);

void vhost_user_can_realize(struct adapter_dev *adev)
{
    VirtIODevice *vdev = &adev->virtio_dev;
    VirtIOMMIOProxy *proxy = &adev->proxy;
    int ret;

    /* Initialize proxy */
    proxy->legacy = 1;

    /* VIRTIO_ID_CAN is 36, check virtio_ids.h in linux*/
    virtio_dev_init(vdev, "virtio-can", 36, sizeof(struct virtio_can_config));
    vdev->user_feature_bits = user_feature_bits;
    vhost_user_can_init(vdev);

    /* add queues */
    vdev->vhucan->vhost_dev->nvqs = 3;
    vdev->vhucan->num_queues = 3;
    vdev->vhucan->queue_size = 64;

    /* NOTE: vdev->vqs == vhucan->virtqs */
    vdev->vqs = (VirtQueue **)malloc(sizeof(VirtQueue *)
                                     * vdev->vhucan->num_queues);
    vdev->vqs[0] = virtio_add_queue(vdev, 64, vhost_user_can_can_handle_output);
    vdev->vqs[1] = virtio_add_queue(vdev, 64, vhost_user_can_can_handle_output);
    vdev->vqs[2] = virtio_add_queue(vdev, 64, vhost_user_can_can_handle_output);

    vdev->vhucan->vhost_vqs = (struct vhost_virtqueue *)malloc(
                                       sizeof(struct vhost_virtqueue) *
                                       vdev->vhucan->num_queues);

    /* Set up vhost device */
    vdev->vhucan->vhost_dev->num_queues = vdev->vhucan->num_queues;
    vdev->vhucan->vhost_dev->nvqs = vdev->vhucan->num_queues;
    vdev->vhucan->vhost_dev->vqs = vdev->vhucan->vhost_vqs;
    vdev->vhucan->vhost_dev->vq_index = 0;
    vdev->vhucan->vhost_dev->backend_features = 0;

    vhost_dev_set_config_notifier(vdev->vhucan->vhost_dev, &can_config_ops);

    /* TODO: Add error handling */
    vhost_dev_init(vdev->vhucan->vhost_dev);

    /* Pass the new obtained features */
    vdev->host_features = vdev->vhucan->vhost_dev->features;

    ret = vhost_dev_get_config(vdev->vhucan->vhost_dev,
                               (uint8_t *)vdev->config,
                               sizeof(struct virtio_can_config));
    if (ret < 0) {
        goto vhost_dev_init_failed;
    }

    vdev->vdev_class->print_config((uint8_t *)&vdev->config);

    return;

vhost_dev_init_failed:
    DBG("vhost_dev_init_failed\n");
    return;
}

static void print_config_can(uint8_t *config_data)
{
    struct virtio_can_config *config_strct =
        (struct virtio_can_config *)config_data;

    (void)config_strct;
    DBG("print_config_can:\n");

    DBG("\tuint16_t status: %u\n", config_strct->status);
}

static void vhost_user_dev_class_init_extra(VirtIODevice *vdev)
{
    DBG("virtio_dev_class_init\n");

    vdev->vdev_class->realize = vhost_user_can_realize;
    vdev->vdev_class->print_config = print_config_can;
}

static void vhost_user_can_init(VirtIODevice *vdev)
{
    DBG("vhost_user_can_init\n");

    VHostUserCan *vhucan = (VHostUserCan *)malloc(sizeof(VHostUserCan));
    vdev->vhucan = vhucan;
    vdev->nvqs = (int *)&vdev->vhdev->nvqs;
    vhucan->parent = vdev;
    vhucan->virtqs = vdev->vqs;
    vhucan->vhost_dev = vdev->vhdev;

    vhost_user_dev_class_init(vdev);
    vhost_user_dev_class_init_extra(vdev);
    virtio_loopback_bus_init(vdev->vbus);
}
