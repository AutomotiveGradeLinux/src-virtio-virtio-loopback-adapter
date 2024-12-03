/*
 *
 * Based on:
 *
 *  1) virtio.c of QEMU project
 *
 *     Copyright IBM, Corp. 2007
 *
 *     Authors:
 *      Anthony Liguori   <aliguori@us.ibm.com>
 *
 *
 *  2) virtio-mmio.c of QEMU project
 *
 *     Copyright (c) 2011 Linaro Limited
 *
 *     Author:
 *      Peter Maydell <peter.maydell@linaro.org>
 *
 *
 * Copyright 2022-2024 Virtual Open Systems SAS.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/param.h>

/* For socket */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

/* Project header files */
#include "virtio_loopback.h"
#include "vhost_user_loopback.h"
#include "virtio_rng.h"

#include <stddef.h>
#include <pthread.h>
#include <limits.h>

#ifdef DEBUG
#define DBG(...) printf("virtio-loopback: " __VA_ARGS__)
#else
#define DBG(...)
#endif /* DEBUG */

void virtio_add_feature(uint64_t *features, unsigned int fbit)
{
    *features |= (1ULL << fbit);
}

bool virtio_has_feature(uint64_t features, unsigned int fbit)
{
    return !!(features & (1ULL << fbit));
}

static int virtio_validate_features(VirtIODevice *vdev)
{
    if (virtio_has_feature(vdev->host_features, VIRTIO_F_IOMMU_PLATFORM) &&
        !virtio_has_feature(vdev->guest_features, VIRTIO_F_IOMMU_PLATFORM)) {
        return -EFAULT;
    }

    return 0;
}

bool virtio_device_should_start(VirtIODevice *vdev, uint8_t status)
{
    if (!vdev->vm_running) {
        return false;
    }

    return virtio_device_started(vdev, status);
}

bool virtio_device_started(VirtIODevice *vdev, uint8_t status)
{

    DBG("virtio_device_started: %d\n", status & VIRTIO_CONFIG_S_DRIVER_OK);
    DBG("status: %d\n", status);

    (void)vdev;

    return status & VIRTIO_CONFIG_S_DRIVER_OK;
}

void virtio_set_started(VirtIODevice *vdev, bool started)
{
    if (started) {
        vdev->start_on_kick = false;
    }

    if (vdev->use_started) {
        vdev->started = started;
    }
}

int virtio_set_status(VirtIODevice *vdev, uint8_t val)
{
    VirtioDeviceClass *k = vdev->vdev_class;

    DBG("virtio_set_status(...)\n");

    if (virtio_has_feature(vdev->guest_features, VIRTIO_F_VERSION_1)) {
        if (!(vdev->status & VIRTIO_CONFIG_S_FEATURES_OK) &&
            val & VIRTIO_CONFIG_S_FEATURES_OK) {
            int ret = virtio_validate_features(vdev);

            if (ret) {
                return ret;
            }
        }
    }

    if ((vdev->status & VIRTIO_CONFIG_S_DRIVER_OK) !=
        (val & VIRTIO_CONFIG_S_DRIVER_OK)) {
        virtio_set_started(vdev, val & VIRTIO_CONFIG_S_DRIVER_OK);
    }

    DBG("set vdev->status:%u\n", vdev->status);

    if (k->set_status) {
        k->set_status(vdev, val);
    }

    vdev->status = val;

    return 0;
}

uint64_t vring_align(uint64_t addr, unsigned long align)
{
    return QEMU_ALIGN_UP(addr, align);
}

uint64_t virtio_queue_get_desc_size(VirtIODevice *vdev, int n)
{
    return sizeof(VRingDesc) * vdev->vq[n].vring.num;
}

uint64_t virtio_queue_get_desc_addr(VirtIODevice *vdev, int n)
{
    return vdev->vq[n].vring.desc;
}

uint64_t virtio_queue_get_avail_addr(VirtIODevice *vdev, int n)
{
    return vdev->vq[n].vring.avail;
}

uint64_t virtio_queue_get_used_addr(VirtIODevice *vdev, int n)
{
    return vdev->vq[n].vring.used;
}

int virtio_queue_get_num(VirtIODevice *vdev, int n)
{
    return vdev->vq[n].vring.num;
}

uint64_t virtio_queue_get_avail_size(VirtIODevice *vdev, int n)
{
    int s;

    s = virtio_has_feature(vdev->guest_features,
                           VIRTIO_RING_F_EVENT_IDX) ? 2 : 0;
    return offsetof(VRingAvail, ring) +
        sizeof(uint16_t) * vdev->vq[n].vring.num + s;
}

uint64_t virtio_queue_get_used_size(VirtIODevice *vdev, int n)
{
    int s;

    s = virtio_has_feature(vdev->guest_features,
                           VIRTIO_RING_F_EVENT_IDX) ? 2 : 0;
    return offsetof(VRingUsed, ring) +
        sizeof(VRingUsedElem) * vdev->vq[n].vring.num + s;
}

/* virt queue functions */
void virtio_queue_update_rings(VirtIODevice *vdev, int n)
{
    VRing *vring = &vdev->vq[n].vring;

    if (!vring->num || !vring->desc || !vring->align) {
        /* not yet setup -> nothing to do */
        return;
    }
    vring->avail = vring->desc + vring->num * sizeof(VRingDesc);
    vring->used = vring_align(vring->avail +
                              offsetof(VRingAvail, ring[vring->num]),
                              vring->align);
}

static uint16_t virtio_queue_split_get_last_avail_idx(VirtIODevice *vdev,
                                                      int n)
{
    return vdev->vq[n].last_avail_idx;
}

unsigned int virtio_queue_get_last_avail_idx(VirtIODevice *vdev, int n)
{
    return virtio_queue_split_get_last_avail_idx(vdev, n);
}

void virtio_queue_set_num(VirtIODevice *vdev, int n, int num)
{
    /*
     * Don't allow guest to flip queue between existent and
     * nonexistent states, or to set it to an invalid size.
     */
    if (!!num != !!vdev->vq[n].vring.num ||
        num > VIRTQUEUE_MAX_SIZE ||
        num < 0) {
        return;
    }
    vdev->vq[n].vring.num = num;
}

uint64_t virtio_queue_get_addr(VirtIODevice *vdev, int n)
{
    return vdev->vq[n].vring.desc;
}

void virtio_queue_set_addr(VirtIODevice *vdev, int n, uint64_t addr)
{
    if (!vdev->vq[n].vring.num) {
        return;
    }
    vdev->vq[n].vring.desc = addr;
    virtio_queue_update_rings(vdev, n);
}

int virtio_queue_ready(VirtQueue *vq)
{
    return vq->vring.avail != 0;
}

uint16_t vring_avail_idx(VirtQueue *vq)
{
    vq->shadow_avail_idx = ((VRingAvail *)vq->vring.avail)->idx;

    return vq->shadow_avail_idx;
}

uint16_t vring_avail_ring(VirtQueue *vq, int i)
{
    return ((VRingAvail *)vq->vring.avail)->ring[i];
}

int virtio_queue_split_empty(VirtQueue *vq)
{
    bool empty;

    if (!vq->vring.avail) {
        return 1;
    }

    if (vq->shadow_avail_idx != vq->last_avail_idx) {
        return 0;
    }

    empty = vring_avail_idx(vq) == vq->last_avail_idx;
    return empty;
}

int virtio_queue_empty(VirtQueue *vq)
{
    return virtio_queue_split_empty(vq);
}

size_t iov_from_buf_full(const struct iovec *iov, unsigned int iov_cnt,
                         size_t offset, const void *buf, size_t bytes)
{
    size_t done;
    unsigned int i;
    for (i = 0, done = 0; (offset || done < bytes) && i < iov_cnt; i++) {
        if (offset < iov[i].iov_len) {
            size_t len = MIN(iov[i].iov_len - offset, bytes - done);
            memcpy(iov[i].iov_base + offset, buf + done, len);
            done += len;
            offset = 0;
        } else {
            offset -= iov[i].iov_len;
        }
    }
    return done;
}

size_t qemu_iov_from_buf(const struct iovec *iov, unsigned int iov_cnt,
             size_t offset, const void *buf, size_t bytes)
{
    if (__builtin_constant_p(bytes) && iov_cnt &&
        offset <= iov[0].iov_len && bytes <= iov[0].iov_len - offset) {
        memcpy(iov[0].iov_base + offset, buf, bytes);
        return bytes;
    } else {
        return iov_from_buf_full(iov, iov_cnt, offset, buf, bytes);
    }
}

/* Called within rcu_read_lock().  */
static inline uint16_t vring_avail_flags(VirtQueue *vq)
{
    return ((VRingAvail *)vq->vring.avail)->flags;
}

/* Called within rcu_read_lock().  */
static inline uint16_t vring_get_used_event(VirtQueue *vq)
{
    return vring_avail_ring(vq, vq->vring.num);
}

/* The following is used with USED_EVENT_IDX and AVAIL_EVENT_IDX */
/*
 * Assuming a given event_idx value from the other side, if
 * we have just incremented index from old to new_idx,
 * should we trigger an event?
 *
 * static inline int vring_need_event(uint16_t event_idx,
 *                                    uint16_t new_idx, uint16_t old)
 * {
 *     /\*
 *      * Note: Xen has similar logic for notification hold-off
 *      * in include/xen/interface/io/ring.h with req_event and req_prod
 *      * corresponding to event_idx + 1 and new_idx respectively.
 *      * Note also that req_event and req_prod in Xen start at 1,
 *      * event indexes in virtio start at 0.
 *      *\/
 *     return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old);
 * }
 */

/* Called within rcu_read_lock(). */
bool virtio_split_should_notify(VirtIODevice *vdev, VirtQueue *vq)
{
    uint16_t old, new;
    bool v;

    /* Always notify when queue is empty (when feature acknowledge) */
    if (virtio_has_feature(vdev->guest_features, VIRTIO_F_NOTIFY_ON_EMPTY) &&
        !vq->inuse && virtio_queue_empty(vq)) {
        return true;
    }

    if (!virtio_has_feature(vdev->guest_features, VIRTIO_RING_F_EVENT_IDX)) {
        return !(vring_avail_flags(vq) & VRING_AVAIL_F_NO_INTERRUPT);
    }

    v = vq->signalled_used_valid;
    vq->signalled_used_valid = true;
    old = vq->signalled_used;
    new = vq->signalled_used = vq->used_idx;
    return !v || vring_need_event(vring_get_used_event(vq), new, old);
}

static inline void vring_used_write(VirtQueue *vq, VRingUsedElem *uelem, int i)
{
    VRingUsed *used = (VRingUsed *)vq->vring.used;

    used->ring[i] = *uelem;
}

void virtqueue_split_fill(VirtQueue *vq, const VirtQueueElement *elem,
                    unsigned int len, unsigned int idx)
{
    VRingUsedElem uelem;

    if (!vq->vring.used) {
        return;
    }

    idx = (idx + vq->used_idx) % vq->vring.num;

    uelem.id = elem->index;
    uelem.len = len;
    vring_used_write(vq, &uelem, idx);
}

void virtqueue_fill(VirtQueue *vq, const VirtQueueElement *elem,
                    unsigned int len, unsigned int idx)
{
    virtqueue_split_fill(vq, elem, len, idx);
}

static inline void vring_used_idx_set(VirtQueue *vq, uint16_t val)
{
    ((VRingUsed *)vq->vring.used)->idx = val;
    vq->used_idx = val;
}

static void virtqueue_split_flush(VirtQueue *vq, unsigned int count)
{
    uint16_t old, new;

    if (!vq->vring.used) {
        return;
    }

    old = vq->used_idx;
    new = old + count;
    vring_used_idx_set(vq, new);
    vq->inuse -= count;
    if ((int16_t)(new - vq->signalled_used) < (uint16_t)(new - old)) {
        vq->signalled_used_valid = false;
    }
}

void virtqueue_flush(VirtQueue *vq, unsigned int count)
{
    virtqueue_split_flush(vq, count);
}

void virtqueue_push(VirtQueue *vq, const VirtQueueElement *elem,
                    unsigned int len)
{
    virtqueue_fill(vq, elem, len, 0);
    virtqueue_flush(vq, 1);
}

void vring_set_avail_event(VirtQueue *vq, uint16_t val)
{
    uint16_t *avail;

    avail = (uint16_t *)&((VRingUsed *)vq->vring.used)->ring[vq->vring.num];
    *avail = val;
}

static bool virtqueue_map_desc(VirtIODevice *vdev, unsigned int *p_num_sg,
                               uint64_t *addr, struct iovec *iov,
                               unsigned int max_num_sg, bool is_write,
                               uint64_t pa, size_t sz)
{
    (void)vdev;
    (void)p_num_sg;
    (void)addr;
    (void)iov;
    (void)max_num_sg;
    (void)is_write;
    (void)pa;
    (void)sz;

    DBG("virtqueue_map_desc: Not implemented\n");

    return true;
}

static void *virtqueue_alloc_element(size_t sz, unsigned out_num,
                                     unsigned in_num)
{
    VirtQueueElement *elem;
    size_t in_addr_ofs = QEMU_ALIGN_UP(sz, __alignof__(elem->in_addr[0]));
    size_t out_addr_ofs = in_addr_ofs + in_num * sizeof(elem->in_addr[0]);
    size_t out_addr_end = out_addr_ofs + out_num * sizeof(elem->out_addr[0]);
    size_t in_sg_ofs = QEMU_ALIGN_UP(out_addr_end, __alignof__(elem->in_sg[0]));
    size_t out_sg_ofs = in_sg_ofs + in_num * sizeof(elem->in_sg[0]);
    size_t out_sg_end = out_sg_ofs + out_num * sizeof(elem->out_sg[0]);

    /*
     * TODO: Add check for requested size
     *
     * assert(sz >= sizeof(VirtQueueElement));
     */
    elem = malloc(out_sg_end);
    elem->out_num = out_num;
    elem->in_num = in_num;
    elem->in_addr = (void *)elem + in_addr_ofs;
    elem->out_addr = (void *)elem + out_addr_ofs;
    elem->in_sg = (void *)elem + in_sg_ofs;
    elem->out_sg = (void *)elem + out_sg_ofs;
    return elem;
}

void *virtqueue_split_pop(VirtQueue *vq, size_t sz)
{
    unsigned int i, head, max;
    VirtIODevice *vdev = vq->vdev;
    VirtQueueElement *elem = NULL;
    unsigned out_num, in_num, elem_entries;
    uint64_t addr[VIRTQUEUE_MAX_SIZE];
    struct iovec iov[VIRTQUEUE_MAX_SIZE];
    VRingDesc *desc;
    int rc;

    if (virtio_queue_split_empty(vq)) {
        goto done;
    }

    /* When we start there are none of either input nor output. */
    out_num = in_num = elem_entries = 0;

    max = vq->vring.num;

    if (vq->inuse >= vq->vring.num) {
        DBG("Virtqueue size exceeded\n");
        goto done;
    }

    if (!virtqueue_get_head(vq, vq->last_avail_idx++, &head)) {
        goto done;
    }

    if (virtio_has_feature(vdev->guest_features, VIRTIO_RING_F_EVENT_IDX)) {
        vring_set_avail_event(vq, vq->last_avail_idx);
    }

    i = head;

    desc = (VRingDesc *)vq->vring.desc + i;

    /* Collect all the descriptors */
    do {
        bool map_ok;

        if (desc->flags & VRING_DESC_F_WRITE) {
            map_ok = virtqueue_map_desc(vdev, &in_num, addr + out_num,
                                        iov + out_num,
                                        VIRTQUEUE_MAX_SIZE - out_num, true,
                                        desc->addr, desc->len);
        } else {
            if (in_num) {
                DBG("Incorrect order for descriptors\n");
                goto err_undo_map;
            }
            map_ok = virtqueue_map_desc(vdev, &out_num, addr, iov,
                                        VIRTQUEUE_MAX_SIZE, false,
                                        desc->addr, desc->len);
        }
        if (!map_ok) {
            goto err_undo_map;
        }

        /* If we've got too many, that implies a descriptor loop. */
        if (++elem_entries > max) {
            goto err_undo_map;
        }

        rc = virtqueue_split_read_next_desc(vdev, desc, max, &i);
    } while (rc == VIRTQUEUE_READ_DESC_MORE);

    if (rc == VIRTQUEUE_READ_DESC_ERROR) {
        goto err_undo_map;
    }

    /* Now copy what we have collected and mapped */
    elem = virtqueue_alloc_element(sz, out_num, in_num);
    elem->index = head;
    elem->ndescs = 1;
    for (i = 0; i < out_num; i++) {
        elem->out_addr[i] = addr[i];
        elem->out_sg[i] = iov[i];
    }
    for (i = 0; i < in_num; i++) {
        elem->in_addr[i] = addr[out_num + i];
        elem->in_sg[i] = iov[out_num + i];
    }

    vq->inuse++;

done:
    return elem;

err_undo_map:
    goto done;
}

void *virtqueue_pop(VirtQueue *vq, size_t sz)
{
    return virtqueue_split_pop(vq, sz);
}

bool virtqueue_get_head(VirtQueue *vq, unsigned int idx,
                               unsigned int *head)
{

    /*
     * Grab the next descriptor number they're advertising, and increment
     * the index we've seen.
     */
    *head = vring_avail_ring(vq, idx % vq->vring.num);

    /* If their number is silly, that's a fatal mistake. */
    if (*head >= vq->vring.num) {
        DBG("Guest says index %u is available", *head);
        return false;
    }

    return true;
}

uint32_t get_vqs_max_size(VirtIODevice *vdev)
{
    uint32_t total_size, temp_size, total_p2 = 1;
    int log_res = 0;

    (void)vdev;

    total_size = VIRTQUEUE_MAX_SIZE * sizeof(VRingDesc);
    total_size += offsetof(VRingAvail, ring) +
                   VIRTQUEUE_MAX_SIZE * sizeof(uint16_t);
    total_size += offsetof(VRingUsed, ring) +
                   VIRTQUEUE_MAX_SIZE * sizeof(uint16_t);

    temp_size = total_size;

    /* Compute log2 of total_size (Needs to be power of 2) */
    while ((temp_size /= 2) > 0) {
        log_res++;
        total_p2 *= 2;
    }

    /* if total_size is not a power of 2: (total_size > 8) -> 16 */
    if (total_size > total_p2) {
        total_size = 2 * total_p2;
    }

    /*
     * Align to page size:  This needed only in case total_size
     * is less than 4096 (PAGE_SIZE)
     */
    if (total_size % PAGE_SIZE > 0) {
        total_size = (total_size / PAGE_SIZE) * PAGE_SIZE + PAGE_SIZE;
    }

    DBG("Total vqs size to mmap is: %u\n", total_size);

    return total_size;
}

int virtqueue_num_heads(VirtQueue *vq, unsigned int idx)
{
    uint16_t num_heads = vring_avail_idx(vq) - idx;

    /* Check it isn't doing very strange things with descriptor numbers. */
    if (num_heads > vq->vring.num) {
        DBG("Guest moved used index from %u to %u",
                     idx, vq->shadow_avail_idx);
        return -EINVAL;
    }

    return num_heads;
}

int virtqueue_split_read_next_desc(VirtIODevice *vdev, VRingDesc *desc,
                                          unsigned int max, unsigned int *next)
{
    (void)vdev;

    /* If this descriptor says it doesn't chain, we're done. */
    if (!(desc->flags & VRING_DESC_F_NEXT)) {
        return VIRTQUEUE_READ_DESC_DONE;
    }

    /* Check they're not leading us off end of descriptors. */
    *next = desc->next;

    if (*next >= max) {
        DBG("Desc next is %u", *next);
        return VIRTQUEUE_READ_DESC_ERROR;
    }

    desc = (VRingDesc *)desc + *next;
    return VIRTQUEUE_READ_DESC_MORE;
}

static void virtqueue_split_get_avail_bytes(VirtQueue *vq,
                            unsigned int *in_bytes, unsigned int *out_bytes,
                            unsigned max_in_bytes, unsigned max_out_bytes)
{
    VirtIODevice *vdev = vq->vdev;
    unsigned int max, idx;
    unsigned int total_bufs, in_total, out_total;
    int rc;

    idx = vq->last_avail_idx;
    total_bufs = in_total = out_total = 0;

    max = vq->vring.num;

    while ((rc = virtqueue_num_heads(vq, idx)) > 0) {
        unsigned int num_bufs;
        VRingDesc *desc;
        unsigned int i;

        num_bufs = total_bufs;

        if (!virtqueue_get_head(vq, idx++, &i)) {
            goto err;
        }

        /* there is no need to copy anything form the cache struct */
        desc = (VRingDesc *)vq->vring.desc + i;

        if (desc->flags & VRING_DESC_F_INDIRECT) {
            if (!desc->len || (desc->len % sizeof(VRingDesc))) {
                DBG("Invalid size for indirect buffer table\n");
                goto err;
            }

            /* If we've got too many, that implies a descriptor loop. */
            if (num_bufs >= max) {
                goto err;
            }
        }

        do {
            /* If we've got too many, that implies a descriptor loop. */
            if (++num_bufs > max) {
                goto err;
            }

            if (desc->flags & VRING_DESC_F_WRITE) {
                in_total += desc->len;
            } else {
                out_total += desc->len;
            }
            if (in_total >= max_in_bytes && out_total >= max_out_bytes) {
                goto done;
            }

            rc = virtqueue_split_read_next_desc(vdev, desc, max, &i);
        } while (rc == VIRTQUEUE_READ_DESC_MORE);

        if (rc == VIRTQUEUE_READ_DESC_ERROR) {
            goto err;
        }

        total_bufs = num_bufs;
    }

    if (rc < 0) {
        goto err;
    }

done:
    if (in_bytes) {
        *in_bytes = in_total;
    }
    if (out_bytes) {
        *out_bytes = out_total;
    }
    return;

err:
    in_total = out_total = 0;
    goto done;
}

void virtqueue_get_avail_bytes(VirtQueue *vq, unsigned int *in_bytes,
                               unsigned int *out_bytes,
                               unsigned max_in_bytes, unsigned max_out_bytes)
{
    if (!vq->vring.desc) {
        goto err;
    }

    virtqueue_split_get_avail_bytes(vq, in_bytes, out_bytes,
                                    max_in_bytes, max_out_bytes);

    return;
err:
    if (in_bytes) {
        *in_bytes = 0;
    }
    if (out_bytes) {
        *out_bytes = 0;
    }
}
