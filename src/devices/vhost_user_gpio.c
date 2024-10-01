/*
 * Based on vhost-user-gpio.c of QEMU project
 *
 * Copyright (c) 2022 Viresh Kumar <viresh.kumar@linaro.org>
 *
 * Copyright (c) 2023 Virtual Open Systems SAS.
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
#include <errno.h>

/* Project header files */
#include "vhost_user_gpio.h"

#ifdef DEBUG
#define DBG(...) printf("vhost-user-gpio: " __VA_ARGS__)
#else
#define DBG(...)
#endif /* DEBUG */

static const int user_feature_bits[] = {
    VIRTIO_F_VERSION_1,
    VIRTIO_F_NOTIFY_ON_EMPTY,
    VIRTIO_RING_F_INDIRECT_DESC,
    VIRTIO_RING_F_EVENT_IDX,
    VIRTIO_GPIO_F_IRQ,
    VIRTIO_F_RING_RESET,
    VHOST_INVALID_FEATURE_BIT
};

static int vu_gpio_config_notifier(struct vhost_dev *dev)
{
    DBG("vu_gpio_config_notifier\n");

    /* TODO: Investigate if we need that */
    //memcpy(dev->vdev->config, &dev->vdev->vhugpio->config, sizeof(gpio->config));

    virtio_notify_config(dev->vdev);

    return 0;
}

const VhostDevConfigOps gpio_ops = {
    .vhost_dev_config_notifier = vu_gpio_config_notifier,
};

static void vu_gpio_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    /*
     * Not normally called; it's the daemon that handles the queue;
     * however virtio's cleanup path can call this.
     */
    DBG("vu_gpio_handle_output not yet implemented\n");
    (void)vdev;
    (void)vq;
}

static void do_vhost_user_cleanup(VirtIODevice *vdev, VHostUserGPIO *gpio)
{
    DBG("do_vhost_user_cleanup not yet implemented\n");
    (void)vdev;
    (void)gpio;
}

static void print_config_gpio(uint8_t *config_data)
{
    struct virtio_gpio_config *config =
        (struct virtio_gpio_config *)config_data;

    (void)config;
    DBG("ngpio: %hu\n", config->ngpio);
    DBG("gpio_names_size: %u\n", config->gpio_names_size);
}

void vhost_user_gpio_init(VirtIODevice *vdev);

void vu_gpio_device_realize(struct adapter_dev *adev)
{
    VirtIODevice *vdev = &adev->virtio_dev;
    VirtIOMMIOProxy *proxy = &adev->proxy;
    int ret;

    DBG("vu_gpio_device_realize()\n");

    /* Initialize proxy */
    proxy->legacy = 1;

    /* VIRTIO_ID_GPIO is 41, check virtio_ids.h in linux */
    virtio_dev_init(vdev, "virtio-gpio", 41,
                sizeof(struct virtio_gpio_config));
    vdev->user_feature_bits = user_feature_bits;

    vhost_user_gpio_init(vdev);
    if (!vdev->vhugpio) {
        DBG("vhugpio memory allocation failed\n");
        goto out_with_error;
    }

    /* add queues */
    vdev->vhugpio->num_queues = 2;
    vdev->vhugpio->queue_size = 64;

    vdev->vqs = (VirtQueue **)malloc(sizeof(VirtQueue *)
                                     * vdev->vhugpio->num_queues);
    vdev->vqs[0] = virtio_add_queue(vdev, 64, vu_gpio_handle_output);
    vdev->vqs[1] = virtio_add_queue(vdev, 64, vu_gpio_handle_output);


    vdev->vhugpio->vhost_vqs = (struct vhost_virtqueue *)
                                malloc(sizeof(struct vhost_virtqueue *));
    if (!vdev->vhugpio->vhost_vqs) {
        DBG("vhost_vqs memory allocation failed\n");
        goto out_with_dev;
    }

    /* Set up vhost device */
    vdev->vhdev->num_queues = vdev->vhugpio->num_queues;
    vdev->vhdev->nvqs = vdev->vhugpio->num_queues;
    vdev->vhdev->vqs = vdev->vhugpio->vhost_vqs;
    vdev->vhdev->vq_index = 0;
    vdev->vhdev->backend_features = 0;

    vhost_dev_set_config_notifier(vdev->vhdev, &gpio_ops);

    /* TODO: Add error handling */
    vhost_dev_init(vdev->vhdev);

    /* Pass the new obtained features */
    vdev->host_features = vdev->vhdev->features;

    ret = vhost_dev_get_config(vdev->vhdev, (uint8_t *)vdev->config,
                               vdev->config_len);
    if (ret < 0) {
        DBG("vu_gpio_realize_connect(): -EPROTO\n");
        do_vhost_user_cleanup(vdev, vdev->vhugpio);
    }

    print_config_gpio((uint8_t *)(vdev->config));
    DBG("(realize completed)\n");

    return;

    /*
     * TODO: Fix the following considering also do_vhost_user_cleanup()
     *
     * out_with_cmd_vq:
     * free(vdev->vhugpio->command_vq);
     */
out_with_dev:
    free(vdev->vhugpio);
out_with_error:
    DBG("Realize funciton return error\n");
    return;
}

static void vhost_user_dev_class_init_extra(VirtIODevice *vdev)
{
    DBG("vu_gpio_class_init()\n");

    vdev->vdev_class->realize = vu_gpio_device_realize;
}

void vhost_user_gpio_init(VirtIODevice *vdev)
{
    DBG("vhost_user_gpio_init(...)\n");

    VHostUserGPIO *vhugpio = (VHostUserGPIO *)malloc(sizeof(VHostUserGPIO));
    if (!vhugpio) {
        DBG("proxy memory allocation failed\n");
        goto out;
    }

    vdev->vhugpio = vhugpio;
    vdev->nvqs = (int *)&vdev->vhdev->nvqs;
    vhugpio->parent = vdev;
    vhugpio->vhost_dev = vdev->vhdev;

    vhost_user_dev_class_init(vdev);
    vhost_user_dev_class_init_extra(vdev);
    virtio_loopback_bus_init(vdev->vbus);

out:
    return;
}
