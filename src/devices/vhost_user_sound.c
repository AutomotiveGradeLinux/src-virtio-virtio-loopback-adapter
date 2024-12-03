/*
 * Based on vhost-user-sound.c of QEMU project
 *
 * Copyright 2020 Red Hat, Inc.
 *
 * Copyright (c) 2023-2024 Virtual Open Systems SAS.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 *
 */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/param.h>

/* Project header files */
#include "vhost_user_sound.h"

#ifdef DEBUG
#define DBG(...) printf("vhost-user-sound: " __VA_ARGS__)
#else
#define DBG(...)
#endif /* DEBUG */

/***************************** vhost-user-sound ******************************/

static const int user_feature_bits[] = {
    VIRTIO_F_VERSION_1,
    VIRTIO_RING_F_INDIRECT_DESC,
    VIRTIO_RING_F_EVENT_IDX,
    VIRTIO_F_RING_RESET,
    VIRTIO_F_NOTIFY_ON_EMPTY,
    VHOST_INVALID_FEATURE_BIT
};

static void vus_snd_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    /*
     * Not normally called; it's the daemon that handles the queue;
     * however virtio's cleanup path can call this.
     */
    (void)vdev;
    (void)vq;
}

static int vus_sound_config_change(struct vhost_dev *dev)
{
    DBG("vus_sound_config_change\n");

    int ret = vhost_dev_get_config(dev, (uint8_t *)dev->vdev->config,
                                   sizeof(struct virtio_snd_config));
    if (ret < 0) {
        DBG("vus_sound_config_change error\n");
        return -1;
    }

    virtio_notify_config(dev->vdev);

    return 0;
}

const VhostDevConfigOps snd_config_ops = {
    .vhost_dev_config_notifier = vus_sound_config_change,
};

static void vhost_user_snd_init(VirtIODevice *vdev);

void vus_device_realize(struct adapter_dev *adev)
{
    VirtIODevice *vdev = &adev->virtio_dev;
    VirtIOMMIOProxy *proxy = &adev->proxy;
    int ret;

    DBG("vus_device_realize\n");

    /* Initialize proxy */
    proxy->legacy = 1;

    /* VIRTIO_ID_SOUND is 25, check virtio_ids.h in linux*/
    virtio_dev_init(vdev, "virtio-sound", 25, sizeof(struct virtio_snd_config));
    vdev->user_feature_bits = user_feature_bits;
    vhost_user_snd_init(vdev);

    /* add queues */
    vdev->vhusnd->vhost_dev->nvqs = 4;
    vdev->vhusnd->num_queues = 4;
    vdev->vhusnd->queue_size = 64;

    /* NOTE: vdev->vqs == vhublk->virtqs */
    vdev->vqs = (VirtQueue **)malloc(sizeof(VirtQueue *)
                                     * vdev->vhusnd->num_queues);
    vdev->vqs[0] = virtio_add_queue(vdev, 64, vus_snd_handle_output);
    vdev->vqs[1] = virtio_add_queue(vdev, 64, vus_snd_handle_output);
    vdev->vqs[2] = virtio_add_queue(vdev, 64, vus_snd_handle_output);
    vdev->vqs[3] = virtio_add_queue(vdev, 64, vus_snd_handle_output);

    vdev->vhusnd->vhost_vqs = (struct vhost_virtqueue *)malloc(
                                       sizeof(struct vhost_virtqueue) *
                                       vdev->vhusnd->num_queues);

    /* Set up vhost device */
    vdev->vhusnd->vhost_dev->num_queues = vdev->vhusnd->num_queues;
    vdev->vhusnd->vhost_dev->nvqs = vdev->vhusnd->num_queues;
    vdev->vhusnd->vhost_dev->vqs = vdev->vhusnd->vhost_vqs;
    vdev->vhusnd->vhost_dev->vq_index = 0;
    vdev->vhusnd->vhost_dev->backend_features = 0;

    vhost_dev_set_config_notifier(vdev->vhusnd->vhost_dev, &snd_config_ops);

    /* TODO: Add error handling */
    vhost_dev_init(vdev->vhusnd->vhost_dev);

    /* Pass the new obtained features */
    vdev->host_features = vdev->vhusnd->vhost_dev->features;

    ret = vhost_dev_get_config(vdev->vhusnd->vhost_dev,
                               (uint8_t *)vdev->config,
                               sizeof(struct virtio_snd_config));
    if (ret < 0) {
        goto vhost_dev_init_failed;
    }

    vdev->vdev_class->print_config((uint8_t *)&vdev->config);

    return;

vhost_dev_init_failed:
    DBG("vhost_dev_init_failed\n");
    return;
}

static void print_config_snd(uint8_t *config_data)
{
    struct virtio_snd_config *config_strct =
        (struct virtio_snd_config *)config_data;

    (void)config_strct;

    DBG("print_config_snd:\n");
    /* # of available physical jacks */
    DBG("\tuint32_t jacks: %u\n", config_strct->jacks);
    /* # of available PCM streams */
    DBG("\tuint32_t streams: %u\n", config_strct->streams);
    /* # of available channel maps */
    DBG("\tuint32_t chmaps: %u\n", config_strct->chmaps);
}

static void vhost_user_dev_class_init_extra(VirtIODevice *vdev)
{
    DBG("vhost_user_dev_class_init_extra\n");

    vdev->vdev_class->realize = vus_device_realize;
    vdev->vdev_class->print_config = print_config_snd;
}

static void vhost_user_snd_init(VirtIODevice *vdev)
{

    DBG("vhost_user_blk_init\n");

    VHostUserSound *vhusnd = (VHostUserSound *)malloc(sizeof(VHostUserSound));
    vdev->vhusnd = vhusnd;
    vdev->nvqs = (int *)&vdev->vhdev->nvqs;
    vhusnd->parent = vdev;
    vhusnd->virtqs = vdev->vqs;
    vhusnd->vhost_dev = vdev->vhdev;

    vhost_user_dev_class_init(vdev);
    vhost_user_dev_class_init_extra(vdev);
    virtio_loopback_bus_init(vdev->vbus);
}
