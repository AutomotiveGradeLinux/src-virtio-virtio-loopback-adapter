/*
 * Based on vhost-user-input.c of QEMU project
 *
 * Copyright (c) 2022-2024 Virtual Open Systems SAS.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/param.h>

/* Project header files */
#include "vhost_user_input.h"

#ifdef DEBUG
#define DBG(...) printf("vhost-user-input: " __VA_ARGS__)
#else
#define DBG(...)
#endif /* DEBUG */

static int vhost_input_config_change(struct vhost_dev *dev)
{
    DBG("vhost-user-input: unhandled backend config change\n");
    (void)dev;

    return -1;
}

const VhostDevConfigOps config_ops = {
    .vhost_dev_config_notifier = vhost_input_config_change,
};

static void vhost_input_set_config(VirtIODevice *vdev, const uint8_t *config_data)
{
    int ret;

    DBG("vhost_input_set_config(...)\n");

    ret = vhost_dev_set_config(vdev->vhdev, config_data,
                               0, sizeof(struct virtio_input_config),
                               VHOST_SET_CONFIG_TYPE_MASTER);
    if (ret < 0) {
        DBG("vhost_input_set_config failed\n");
        return;
    }

    virtio_notify_config(vdev);
}

static void print_config_input(uint8_t *config_data)
{
    struct virtio_input_config *config_strct =
        (struct virtio_input_config *)config_data;

    DBG("print_config_input: Not yet implemented\n");
    (void)config_strct;
}

static void virtio_input_handle(VirtIODevice *vdev, VirtQueue *vq)
{
    DBG("virtio_input_handle_evt(...) not yet implemeted\n");
    (void)vdev;
    (void)vq;
}

static const int user_feature_bits[] = {
    VIRTIO_F_VERSION_1,
    VIRTIO_RING_F_INDIRECT_DESC,
    VIRTIO_RING_F_EVENT_IDX,
    VIRTIO_F_RING_RESET,
    VIRTIO_F_NOTIFY_ON_EMPTY,
    VHOST_INVALID_FEATURE_BIT
};

static void vhost_user_input_init(VirtIODevice *vdev);

void virtio_input_device_realize(struct adapter_dev *adev)
{
    VirtIODevice *vdev = &adev->virtio_dev;
    VirtIOMMIOProxy *proxy = &adev->proxy;

    int nvqs = 2; /* qemu choice: 2 */

    DBG("virtio_input_device_realize(...)\n");

    /* Initialize proxy */
    proxy->legacy = 1;

    virtio_dev_init(vdev, "virtio-input", 18, sizeof(struct virtio_input_config));
    vdev->user_feature_bits = user_feature_bits;
    vhost_user_input_init(vdev);

    vdev->vqs = (struct VirtQueue **)malloc(
                                     sizeof(struct VirtQueue *) * nvqs);

    vdev->vqs[0] = virtio_add_queue(vdev, 64, virtio_input_handle);
    vdev->vqs[1] = virtio_add_queue(vdev, 64, virtio_input_handle);

    vhost_dev_set_config_notifier(vdev->vhdev,
                                  &config_ops);

    vdev->vhdev->nvqs = nvqs;
    vdev->vhdev->num_queues = nvqs;
    vdev->vhdev->vq_index = 0;
    vdev->vhdev->backend_features = 0;
    vdev->vhdev->vqs = (struct vhost_virtqueue *)malloc(
                                    sizeof(struct vhost_virtqueue) * nvqs);

    vhost_dev_init(vdev->vhdev);

    /* Pass the new obtained features */
    vdev->host_features = vdev->vhuinput->vhost_dev->features;

    DBG("final features: 0x%lx\n", vdev->host_features);
}

static void vhost_user_dev_class_init_extra(VirtIODevice *vdev)
{

    DBG("vhost_user_dev_class_init_extra(...)\n");

    vdev->vdev_class->realize = virtio_input_device_realize;
    vdev->vdev_class->set_config = vhost_input_set_config;
    vdev->vdev_class->print_config = print_config_input;
}

static void vhost_user_input_init(VirtIODevice *vdev)
{
    DBG("vhost_user_input_init(...)\n");

    VhostUserInput *vhuinput = (VhostUserInput *)malloc(sizeof(VhostUserInput));
    vdev->vhuinput = vhuinput;
    vdev->nvqs = (int*)&vdev->vhdev->nvqs;
    vhuinput->parent = vdev;
    vhuinput->vhost_dev = vdev->vhdev;

    /*
     * Call first the virtio_input class init to set up
     * the basic functionality.
     */
    vhost_user_dev_class_init(vdev);
    vhost_user_dev_class_init_extra(vdev);

    /* finally initialize the bus */
    virtio_loopback_bus_init(vdev->vbus);
}

