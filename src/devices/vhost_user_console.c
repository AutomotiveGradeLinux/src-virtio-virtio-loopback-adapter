/*
 * Copyright (c) 2023 Virtual Open Systems SAS.
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
#include "vhost_user_console.h"

#ifdef DEBUG
#define DBG(...) printf("vhost-user-console: " __VA_ARGS__)
#else
#define DBG(...)
#endif /* DEBUG */

static const int user_feature_bits[] = {
    VIRTIO_F_VERSION_1,
    VIRTIO_RING_F_INDIRECT_DESC,
    VIRTIO_RING_F_EVENT_IDX,
    VIRTIO_F_RING_RESET,
    VIRTIO_F_NOTIFY_ON_EMPTY,
    VHOST_INVALID_FEATURE_BIT,
    VIRTIO_CONSOLE_F_SIZE,
    VIRTIO_CONSOLE_F_MULTIPORT,
    VIRTIO_CONSOLE_F_EMERG_WRITE
};

static void vhost_user_console_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    /*
     * Not normally called; it's the daemon that handles the queue;
     * however virtio's cleanup path console call this.
     */
    DBG("vhost_user_console_handle_output: Not yet implemented!\n");
    (void)vdev;
    (void)vq;
}

static int vhost_user_console_config_change(struct vhost_dev *dev)
{
    DBG("vhost_user_console_console_config_change\n");

    int ret = vhost_dev_get_config(dev, (uint8_t *)dev->vdev->config,
                                   sizeof(struct virtio_console_config));
    if (ret < 0) {
        DBG("vhost_user_console_sound_config_change error\n");
        return -1;
    }

    virtio_notify_config(dev->vdev);

    return 0;
}

const VhostDevConfigOps console_config_ops = {
    .vhost_dev_config_notifier = vhost_user_console_config_change,
};

static void vhost_user_console_init(VirtIODevice *vdev);

void vhost_user_console_realize(struct adapter_dev *adev)
{
    VirtIODevice *vdev = &adev->virtio_dev;
    VirtIOMMIOProxy *proxy = &adev->proxy;
    int ret;

    DBG("vhost_user_console_device_realize\n");

    /* Initialize proxy */
    proxy->legacy = 1;

    /* VIRTIO_ID_CONSOLE is 3, check virtio_ids.h in linux*/
    virtio_dev_init(vdev, "virtio-console", 3, sizeof(struct virtio_console_config));
    vdev->user_feature_bits = user_feature_bits;
    vhost_user_console_init(vdev);

    /* add queues */
    vdev->vhuconsole->vhost_dev->nvqs = 4;
    vdev->vhuconsole->num_queues = 4;
    vdev->vhuconsole->queue_size = 128;

    /* NOTE: vdev->vqs == vhuconsole->virtqs */
    vdev->vqs = (VirtQueue **)malloc(sizeof(VirtQueue *)
                                     * vdev->vhuconsole->num_queues);
    vdev->vqs[0] = virtio_add_queue(vdev, 128, vhost_user_console_handle_output);
    vdev->vqs[1] = virtio_add_queue(vdev, 128, vhost_user_console_handle_output);
    vdev->vqs[2] = virtio_add_queue(vdev, 128, vhost_user_console_handle_output);
    vdev->vqs[3] = virtio_add_queue(vdev, 128, vhost_user_console_handle_output);

    vdev->vhuconsole->vhost_vqs = (struct vhost_virtqueue *)malloc(
                                       sizeof(struct vhost_virtqueue) *
                                       vdev->vhuconsole->num_queues);

    /* Set up vhost device */
    vdev->vhuconsole->vhost_dev->num_queues = vdev->vhuconsole->num_queues;
    vdev->vhuconsole->vhost_dev->nvqs = vdev->vhuconsole->num_queues;
    vdev->vhuconsole->vhost_dev->vqs = vdev->vhuconsole->vhost_vqs;
    vdev->vhuconsole->vhost_dev->vq_index = 0;
    vdev->vhuconsole->vhost_dev->backend_features = 0;

    vhost_dev_set_config_notifier(vdev->vhuconsole->vhost_dev,
                                  &console_config_ops);

    /* TODO: Add error handling */
    vhost_dev_init(vdev->vhuconsole->vhost_dev);

    /* Pass the new obtained features */
    vdev->host_features = vdev->vhuconsole->vhost_dev->features;

    ret = vhost_dev_get_config(vdev->vhdev,
                               (uint8_t *)vdev->config,
                               sizeof(struct virtio_console_config));
    if (ret < 0) {
        goto vhost_dev_init_failed;
    }

    vdev->vdev_class->print_config((uint8_t *)&vdev->config);

    return;

vhost_dev_init_failed:
    DBG("vhost_dev_init_failed\n");
    return;
}

static void print_config_console(uint8_t *config_data)
{
    struct virtio_console_config *config_strct =
        (struct virtio_console_config *)config_data;

    (void)config_strct;
    DBG("print_config_console:\n");
    DBG("\tuint16_t cols: %u\n", config_strct->cols);
    DBG("\tuint16_t rows: %u\n", config_strct->rows);
    DBG("\tuint16_t max_nr_ports: %u\n", config_strct->max_nr_ports);
    DBG("\tuint16_t emerg_wr: %u\n", config_strct->emerg_wr);
}

static void vhost_user_dev_class_init_extra(VirtIODevice *vdev)
{
    DBG("virtio_dev_class_init\n");

    vdev->vdev_class->realize = vhost_user_console_realize;
    vdev->vdev_class->print_config = print_config_console;
}

static void vhost_user_console_init(VirtIODevice *vdev)
{

    DBG("vhost_user_console_init\n");

    VHostUserConsole *vhuconsole =
                        (VHostUserConsole *)malloc(sizeof(VHostUserConsole));
    vdev->vhuconsole = vhuconsole;
    vdev->nvqs = (int *)&vdev->vhdev->nvqs;
    vhuconsole->parent = vdev;
    vhuconsole->virtqs = vdev->vqs;
    vhuconsole->vhost_dev = vdev->vhdev;

    vhost_user_dev_class_init(vdev);
    vhost_user_dev_class_init_extra(vdev);
    virtio_loopback_bus_init(vdev->vbus);
}
