/*
 * Based on vhost-user-blk.c of QEMU project
 *
 *   Copyright(C) 2017 Intel Corporation.
 *
 *   Authors:
 *    Changpeng Liu <changpeng.liu@intel.com>
 *
 *   Largely based on the "vhost-user-scsi.c" and "vhost-scsi.c" implemented by:
 *   Felipe Franciosi <felipe@nutanix.com>
 *   Stefan Hajnoczi <stefanha@linux.vnet.ibm.com>
 *   Nicholas Bellinger <nab@risingtidesystems.com>
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
#include "vhost_user_blk.h"

#ifdef DEBUG
#define DBG(...) printf("vhost-user-blk: " __VA_ARGS__)
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
    VIRTIO_BLK_F_SEG_MAX,
    VIRTIO_BLK_F_GEOMETRY,
    VIRTIO_BLK_F_TOPOLOGY,
    VIRTIO_BLK_F_FLUSH,
    VIRTIO_BLK_F_DISCARD,
    VIRTIO_BLK_F_WRITE_ZEROES,
    VIRTIO_BLK_F_BLK_SIZE,
    VIRTIO_BLK_F_RO,
    VIRTIO_BLK_F_CONFIG_WCE,
    VIRTIO_BLK_F_MQ,
};

static void vhost_user_blk_start(VirtIODevice *vdev)
{
    VHostUserBlk *s = vdev->vhublk;
    VirtioBus *k = vdev->vbus;
    unsigned int i;
    int ret;

    DBG("vhost_user_blk_start\n");

    if (!k->set_guest_notifiers) {
        DBG("binding does not support guest notifiers\n");
        goto error;
    }

    ret = vhost_dev_enable_notifiers(s->vhost_dev, vdev);
    if (ret < 0) {
        DBG("Error enabling host notifiers\n");
        goto error;
    }

    ret = k->set_guest_notifiers(k->vdev, s->vhost_dev->nvqs, true);
    if (ret < 0) {
        DBG("Error enabling host notifier\n");
        goto error;
    }

    s->vhost_dev->acked_features = vdev->guest_features;
    DBG("acked_features: 0x%lx\n", vdev->guest_features);

    /* FIXME: We might do not need that */
    ret = vhost_dev_prepare_inflight(s->vhost_dev, vdev);
    if (ret < 0) {
        DBG("Error setting inflight format\n");
        goto error;
    }

    if (!s->inflight->addr) {
        ret = vhost_dev_get_inflight(s->vhost_dev, s->queue_size, s->inflight);
        if (ret < 0) {
            DBG("Error getting inflight\n");
            goto error;
        }
    }

    ret = vhost_dev_set_inflight(s->vhost_dev, s->inflight);
    if (ret < 0) {
        DBG("Error setting inflight\n");
        goto error;
    }

    ret = vhost_dev_start(s->vhost_dev, vdev, true);
    if (ret < 0) {
        DBG("Error starting vhost\n");
        goto error;
    }

    s->started_vu = true;

    /*
     * guest_notifier_mask/pending not used yet, so just unmask
     * everything here. virtio-pci will do the right thing by
     * enabling/disabling irqfd.
     */
    for (i = 0; i < s->vhost_dev->nvqs; i++) {
        vhost_virtqueue_mask(s->vhost_dev, vdev, i, false);
    }

    DBG("vhost_user_blk_start returns successfully\n");
    return;

error:

}

static int vhost_user_blk_handle_config_change(struct vhost_dev *dev)
{
    int ret;
    struct virtio_blk_config blkcfg;
    VHostUserBlk *s = dev->vdev->vhublk;

    DBG("vhost_user_blk_handle_config_change(...)\n");

    ret = vhost_dev_get_config(dev, (uint8_t *)&blkcfg,
                               sizeof(struct virtio_blk_config));
    if (ret < 0) {
        return ret;
    }

    /* valid for resize only */
    if (blkcfg.capacity != s->blkcfg.capacity) {
        DBG("blkcfg.capacity != s->blkcfg.capacity\n");
        s->blkcfg.capacity = blkcfg.capacity;
        memcpy(dev->vdev->config, &s->blkcfg, sizeof(struct virtio_blk_config));
        virtio_notify_config(dev->vdev);
    }

    return 0;
}

const VhostDevConfigOps blk_ops = {
    .vhost_dev_config_notifier = vhost_user_blk_handle_config_change,
};

/*
 * These functions will be used in the future:
 *
 * static void vhost_user_blk_disconnect(VirtIODevice *dev)
 * {
 *     DBG("vhost_user_blk_disconnect not yet implemented\n");
 *     (void)dev;
 * }
 *
 */

static void vhost_user_blk_update_config(VirtIODevice *vdev, uint8_t *config)
{
    VHostUserBlk *s = vdev->vhublk;

    DBG("vhost_user_blk_update_config(...)\n");

    /* Our num_queues overrides the device backend */
    memcpy(&s->blkcfg.num_queues, &s->num_queues, sizeof(uint64_t));

    memcpy(config, &s->blkcfg, sizeof(struct virtio_blk_config));
}

static void vhost_user_blk_set_config(VirtIODevice *vdev, const uint8_t *config)
{
    VHostUserBlk *s = vdev->vhublk;
    struct virtio_blk_config *blkcfg = (struct virtio_blk_config *)config;
    int ret;

    DBG("vhost_user_blk_set_config(...)\n");

    if (blkcfg->wce == s->blkcfg.wce) {
        DBG("blkcfg->wce == s->blkcfg.wce\n");
        return;
    }

    ret = vhost_dev_set_config(s->vhost_dev, &blkcfg->wce,
                               offsetof(struct virtio_blk_config, wce),
                               sizeof(blkcfg->wce),
                               VHOST_SET_CONFIG_TYPE_MASTER);
    if (ret < 0) {
        DBG("set device config space failed\n");
        return;
    }

    s->blkcfg.wce = blkcfg->wce;
}

static void vhost_user_blk_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    /*
     * Not normally called; it's the daemon that handles the queue;
     * however virtio's cleanup path can call this.
     */
    DBG("vhost_user_blk_handle_output not yet implemented\n");
    (void)vdev;
    (void)vq;
}

void vhost_user_blk_init(VirtIODevice *vdev);
static void print_config_blk(uint8_t *config_data);

void vhost_user_blk_realize(struct adapter_dev *adev)
{
    struct VirtIODevice *vdev = &adev->virtio_dev;
    unsigned int queue_size = adev->device_params->queue_size;
    unsigned int queue_num = adev->device_params->queue_num;
    VirtIOMMIOProxy *proxy = &adev->proxy;
    int ret, i;

    DBG("vhost_user_blk_realize\n");

    /* Initialize proxy */
    proxy->legacy = 1;

    /* VIRTIO_ID_BLOCK is 2, check virtio_ids.h in linux */
    virtio_dev_init(vdev, "virtio-blk", 2,
                sizeof(struct virtio_blk_config));
    vdev->user_feature_bits = user_feature_bits;

    vhost_user_blk_init(vdev);

    vdev->vhublk->config_wce = 1;

    vdev->vhublk->vhost_dev->nvqs = queue_num;
    vdev->vhublk->num_queues = queue_num;
    vdev->vhublk->queue_size = queue_size;

    /* NOTE: vdev->vqs == vhublk->virtqs */
    vdev->vqs = (VirtQueue **)malloc(sizeof(VirtQueue *)
                                            * vdev->vhublk->num_queues);
    for (i = 0; i < vdev->vhublk->num_queues; i++) {
        vdev->vqs[i] = virtio_add_queue(vdev,
                                        vdev->vhublk->queue_size,
                                        vhost_user_blk_handle_output);
    }

    vdev->vhublk->inflight = (struct vhost_inflight *)malloc(
                                     sizeof(struct vhost_inflight));
    vdev->vhublk->vhost_vqs = (struct vhost_virtqueue *)malloc(
                                            sizeof(struct vhost_virtqueue) *
                                            vdev->vhublk->num_queues);

    vdev->vhublk->vhost_dev->num_queues = vdev->vhublk->num_queues;
    vdev->vhublk->vhost_dev->nvqs = vdev->vhublk->num_queues;
    vdev->vhublk->vhost_dev->vqs = vdev->vhublk->vhost_vqs;
    vdev->vhublk->vhost_dev->vq_index = 0;
    vdev->vhublk->vhost_dev->backend_features = 0;

    vhost_dev_set_config_notifier(vdev->vhublk->vhost_dev, &blk_ops);

    vhost_dev_init(vdev->vhublk->vhost_dev);

    /* Pass the new obtained features */
    vdev->host_features = vdev->vhublk->vhost_dev->features;

    /*
     * The next line disables VIRTIO_RING_F_INDIRECT_DESC:
     * vdev->host_features &= ~(1ULL << VIRTIO_RING_F_INDIRECT_DESC);
     */

    ret = vhost_dev_get_config(vdev->vhublk->vhost_dev, (uint8_t *)&vdev->vhublk->blkcfg,
                               sizeof(struct virtio_blk_config));
    if (ret < 0) {
        DBG("vhost_dev_get_config failed\n");
        return;
    }

    if (vdev->vhublk->config_wce != vdev->vhublk->blkcfg.wce) {
        vdev->vhublk->blkcfg.wce = vdev->vhublk->config_wce;

        ret = vhost_dev_set_config(vdev->vhublk->vhost_dev, (uint8_t *)&vdev->vhublk->blkcfg.wce,
                                   offsetof(struct virtio_blk_config, wce),
                                   sizeof(vdev->vhublk->blkcfg.wce),
                                   VHOST_SET_CONFIG_TYPE_MASTER);
        if (ret < 0) {
            DBG("set device config space failed\n");
            return;
        }
    }

    DBG("final vdev->host_features: 0x%lx\n",
         vdev->host_features);

    print_config_blk((uint8_t *)(&vdev->vhublk->blkcfg));
}

static void vhost_user_dev_class_init_extra(VirtIODevice *vdev)
{
    DBG("virtio_dev_class_init\n");

    vdev->vdev_class->realize = vhost_user_blk_realize;
    vdev->vdev_class->start = vhost_user_blk_start;
    vdev->vdev_class->get_config = vhost_user_blk_update_config;
    vdev->vdev_class->set_config = vhost_user_blk_set_config;
    vdev->vdev_class->print_config = print_config_blk;
}

void vhost_user_blk_init(VirtIODevice *vdev)
{
    struct vhost_dev *vhdev = vdev->vhdev;
    DBG("vhost_user_blk_init\n");

    VHostUserBlk *vhublk = (VHostUserBlk *)malloc(sizeof(VHostUserBlk));
    vdev->vhublk = vhublk;
    vdev->nvqs = (int *)&vhdev->nvqs;
    vhublk->parent = vdev;
    vhublk->virtqs = vdev->vqs;
    vhublk->vhost_dev = vhdev;

    vhost_user_dev_class_init(vdev);
    vhost_user_dev_class_init_extra(vdev);
    virtio_loopback_bus_init(vdev->vbus);
}

static void print_config_blk(uint8_t *config_data)
{
    struct virtio_blk_config *config_strct =
        (struct virtio_blk_config *)config_data;

    (void)config_strct;
    DBG("uint64_t capacity: %llu\n", config_strct->capacity);
    DBG("uint32_t size_max: %u\n", config_strct->size_max);
    DBG("uint32_t seg_max: %u\n", config_strct->seg_max);

    DBG("virtio_blk_geometry:\n");
    DBG("    uint16_t cylinders: %u\n",
            config_strct->geometry.cylinders);
    DBG("    uint8_t heads: %u\n",
            config_strct->geometry.heads);
    DBG("    uint8_t sectors: %u\n",
            config_strct->geometry.sectors);

    DBG("uint32_t blk_size: %u\n", config_strct->blk_size);
    DBG("uint8_t physical_block_exp: %u\n",
            config_strct->physical_block_exp);
    DBG("uint8_t alignment_offset: %u\n",
            config_strct->alignment_offset);
    DBG("uint16_t min_io_size: %u\n", config_strct->min_io_size);
    DBG("uint32_t opt_io_size: %u\n", config_strct->opt_io_size);
    DBG("uint8_t wce: %u\n", config_strct->wce);
    DBG("uint8_t unused: %u\n", config_strct->unused);
    DBG("uint16_t num_queues: %u\n", config_strct->num_queues);
    DBG("uint32_t max_discard_sectors: %u\n",
            config_strct->max_discard_sectors);
    DBG("uint32_t max_discard_seg: %u\n", config_strct->max_discard_seg);
    DBG("uint32_t discard_sector_alignment: %u\n",
            config_strct->discard_sector_alignment);
    DBG("uint32_t max_write_zeroes_sectors: %u\n",
            config_strct->max_write_zeroes_sectors);
    DBG("uint32_t max_write_zeroes_seg: %u\n",
            config_strct->max_write_zeroes_seg);
    DBG("uint8_t write_zeroes_may_unmap: %u\n",
            config_strct->write_zeroes_may_unmap);
    DBG("uint8_t unused1[3]: %u\n", config_strct->unused1[0]);
    DBG("uint8_t unused1[3]: %u\n", config_strct->unused1[1]);
    DBG("uint8_t unused1[3]: %u\n", config_strct->unused1[2]);
}
