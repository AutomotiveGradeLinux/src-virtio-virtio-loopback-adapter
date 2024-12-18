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

void print_neg_flag(uint64_t neg_flag, bool read)
{
    if (read) {
        DBG("Read:\t");
    } else {
        DBG("Write:\t");
    }

    switch (neg_flag) {
    case VIRTIO_MMIO_MAGIC_VALUE:           /* 0x000 */
        DBG("VIRTIO_MMIO_MAGIC_VALUE\n");
        break;
    case VIRTIO_MMIO_VERSION:               /* 0x004 */
        DBG("VIRTIO_MMIO_VERSION\n");
        break;
    case VIRTIO_MMIO_DEVICE_ID:             /* 0x008 */
        DBG("VIRTIO_MMIO_DEVICE_ID\n");
        break;
    case VIRTIO_MMIO_VENDOR_ID:             /* 0x00c */
        DBG("VIRTIO_MMIO_VENDOR_ID\n");
        break;
    case VIRTIO_MMIO_DEVICE_FEATURES:       /* 0x010 */
        DBG("VIRTIO_MMIO_DEVICE_FEATURES\n");
        break;
    case VIRTIO_MMIO_DEVICE_FEATURES_SEL:   /* 0x014 */
        DBG("VIRTIO_MMIO_DEVICE_FEATURES_SEL\n");
        break;
    case VIRTIO_MMIO_DRIVER_FEATURES:       /* 0x020 */
        DBG("VIRTIO_MMIO_DRIVER_FEATURES\n");
        break;
    case VIRTIO_MMIO_DRIVER_FEATURES_SEL:   /* 0x024 */
        DBG("VIRTIO_MMIO_DRIVER_FEATURES_SEL\n");
        break;
    case VIRTIO_MMIO_GUEST_PAGE_SIZE:       /* 0x028 */
        DBG("VIRTIO_MMIO_GUEST_PAGE_SIZE\n");
        break;
    case VIRTIO_MMIO_QUEUE_SEL:             /* 0x030 */
        DBG("VIRTIO_MMIO_QUEUE_SEL\n");
        break;
    case VIRTIO_MMIO_QUEUE_NUM_MAX:         /* 0x034 */
        DBG("VIRTIO_MMIO_QUEUE_NUM_MAX\n");
        break;
    case VIRTIO_MMIO_QUEUE_NUM:             /* 0x038 */
        DBG("VIRTIO_MMIO_QUEUE_NUM\n");
        break;
    case VIRTIO_MMIO_QUEUE_ALIGN:           /* 0x03c */
        DBG("VIRTIO_MMIO_QUEUE_ALIGN\n");
        break;
    case VIRTIO_MMIO_QUEUE_PFN:             /* 0x040 */
        DBG("VIRTIO_MMIO_QUEUE_PFN\n");
        break;
    case VIRTIO_MMIO_QUEUE_READY:           /* 0x044 */
        DBG("VIRTIO_MMIO_QUEUE_READY\n");
        break;
    case VIRTIO_MMIO_QUEUE_NOTIFY:          /* 0x050 */
        DBG("VIRTIO_MMIO_QUEUE_NOTIFY\n");
        break;
    case VIRTIO_MMIO_INTERRUPT_STATUS:      /* 0x060 */
        DBG("VIRTIO_MMIO_INTERRUPT_STATUS\n");
        break;
    case VIRTIO_MMIO_INTERRUPT_ACK:         /* 0x064 */
        DBG("VIRTIO_MMIO_INTERRUPT_ACK\n");
        break;
    case VIRTIO_MMIO_STATUS:                /* 0x070 */
        DBG("VIRTIO_MMIO_STATUS\n");
        break;
    case VIRTIO_MMIO_QUEUE_DESC_LOW:        /* 0x080 */
        DBG("VIRTIO_MMIO_QUEUE_DESC_LOW\n");
        break;
    case VIRTIO_MMIO_QUEUE_DESC_HIGH:       /* 0x084 */
        DBG("VIRTIO_MMIO_QUEUE_DESC_HIGH\n");
        break;
    case VIRTIO_MMIO_QUEUE_AVAIL_LOW:       /* 0x090 */
        DBG("VIRTIO_MMIO_QUEUE_AVAIL_LOW\n");
        break;
    case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:      /* 0x094 */
        DBG("VIRTIO_MMIO_QUEUE_AVAIL_HIGH\n");
        break;
    case VIRTIO_MMIO_QUEUE_USED_LOW:        /* 0x0a0 */
        DBG("VIRTIO_MMIO_QUEUE_USED_LOW\n");
        break;
    case VIRTIO_MMIO_QUEUE_USED_HIGH:       /* 0x0a4 */
        DBG("VIRTIO_MMIO_QUEUE_USED_HIGH\n");
        break;
    case VIRTIO_MMIO_SHM_SEL:               /* 0x0ac */
        DBG("VIRTIO_MMIO_SHM_SEL\n");
        break;
    case VIRTIO_MMIO_SHM_LEN_LOW:           /* 0x0b0 */
        DBG("VIRTIO_MMIO_SHM_LEN_LOW\n");
        break;
    case VIRTIO_MMIO_SHM_LEN_HIGH:          /* 0x0b4 */
        DBG("VIRTIO_MMIO_SHM_LEN_HIGH\n");
        break;
    case VIRTIO_MMIO_SHM_BASE_LOW:          /* 0x0b8 */
        DBG("VIRTIO_MMIO_SHM_BASE_LOW\n");
        break;
    case VIRTIO_MMIO_SHM_BASE_HIGH:         /* 0x0bc */
        DBG("VIRTIO_MMIO_SHM_BASE_HIGH\n");
        break;
    case VIRTIO_MMIO_CONFIG_GENERATION:     /* 0x0fc */
        DBG("VIRTIO_MMIO_CONFIG_GENERATION\n");
        break;
    default:
        if (neg_flag >= VIRTIO_MMIO_CONFIG) {
            DBG("\tVIRTIO_MMIO_CONFIG\n");
        } else {
            DBG("\tNegotiation flag Unknown: %lu\n", neg_flag);
        }
        return;
    }
}

/* Called within rcu_read_lock().  */
static bool virtio_should_notify(VirtIODevice *vdev, VirtQueue *vq)
{
    return virtio_split_should_notify(vdev, vq);
}

void virtio_set_isr(VirtIODevice *vdev, int value)
{
    uint8_t old = vdev->isr;

    /*
     * Do not write ISR if it does not change, so that its cacheline remains
     * shared in the common case where the guest does not read it.
     */
    if ((old & value) != value) {
        vdev->isr |= value;
    }

    DBG("Update isr: %d\n", vdev->isr);
}

static void virtio_irq(VirtQueue *vq)
{
    pthread_mutex_lock(&vq->vdev->isr_lock);
    virtio_set_isr(vq->vdev, 0x1);
    pthread_mutex_unlock(&vq->vdev->isr_lock);
    virtio_notify_vector(vq->vdev);
}

void virtio_notify_config(VirtIODevice *vdev)
{

    DBG("virtio_notify_config\n");

    if (!(vdev->status & VIRTIO_CONFIG_S_DRIVER_OK)) {
        return;
    }

    pthread_mutex_lock(&vdev->isr_lock);
    virtio_set_isr(vdev, 0x3);
    pthread_mutex_unlock(&vdev->isr_lock);
    vdev->generation++;
    /*
     * MMIO does not use vector parameter:
     * virtio_notify_vector(vdev, vdev->config_vector);
     */
    virtio_notify_vector(vdev);
}

void virtio_loopback_update_irq(VirtIODevice *vdev)
{
    int level, irq_num = 44;
    struct adapter_dev *adev = to_adapter_device(vdev, virtio_dev);

    if (!vdev) {
        return;
    }

    level = (vdev->isr != 0);

    if (level == 1) {
        DBG("Trigger interrupt (ioctl)\n");
        DBG("Interrupt counter: %d\n", vdev->int_count++);
        (void) ioctl(adev->loopback_fd, IRQ, &irq_num);
    } else {
        DBG("No interrupt\n");
    }
}

bool virtio_device_disabled(VirtIODevice *vdev)
{
    return vdev->disabled || vdev->broken;
}

/* virtio device */
void virtio_notify_vector(VirtIODevice *vdev)
{

    /* TODO: Check if this is still needed */
    if (virtio_device_disabled(vdev)) {
        DBG("Device is disabled\n");
        return;
    }

    virtio_loopback_update_irq(vdev);

    /*
     * TODO: substitue the previous line with the
     *       following when it's implemented
     *
     * if (k->notify) {
     *     k->notify(qbus->parent, vector);
     * }
     */
}

void virtio_notify(VirtIODevice *vdev, VirtQueue *vq)
{
    if (!virtio_should_notify(vdev, vq)) {
        DBG("Do not notify!\n");
        return;
    }

    virtio_irq(vq);
}

int virtio_set_features_nocheck(VirtIODevice *vdev, uint64_t val)
{
    bool bad = (val & ~(vdev->host_features)) != 0;

    val &= vdev->host_features;

    vdev->guest_features |= val;
    return bad ? -1 : 0;
}

int virtio_set_features(VirtIODevice *vdev, uint64_t val)
{
    int ret;
    /*
     * The driver must not attempt to set features after feature negotiation
     * has finished.
     */
    if (vdev->status & VIRTIO_CONFIG_S_FEATURES_OK) {
        DBG("virtio_set_features: vdev->status "
            "& VIRTIO_CONFIG_S_FEATURES_OK\n");
        return -EINVAL;
    }
    ret = virtio_set_features_nocheck(vdev, val);
    return ret;
}

static void virtio_queue_guest_notifier_read(EventNotifier *n)
{
    VirtQueue *vq = container_of(n, VirtQueue, guest_notifier);
    if (event_notifier_test_and_clear(n)) {
        virtio_irq(vq);
    }
}

void *loopback_event_select(void *_e)
{
    int retval;
    fd_set rfds;
    EventNotifier *e = (EventNotifier *)_e;
    int rfd = e->rfd;
    VirtQueue *vq = container_of(e, VirtQueue, guest_notifier);
    VirtIODevice *vdev = vq->vdev;
    struct adapter_dev *adev = to_adapter_device(vdev, virtio_dev);

    DBG("\nWaiting event from vhost-user-device\n");

    FD_ZERO(&rfds);
    FD_SET(rfd, &rfds);

    while (1) {
        retval = select(rfd + 1, &rfds, NULL, NULL, NULL);
        if (retval == -1) {
            DBG("select() error. Exiting...\n");
            exit(1);
        }

        if (retval > 0) {
            if (pthread_mutex_lock(&vdev->interrupt_lock) != 0) {
                printf("[ERROR] Locking failed\n");
                exit(1);
            }
            DBG("\n\nEvent has come from the vhost-user-device "
                "(eventfd: %d) -> event_count: %d (select value: %d)\n\n",
                                              rfd, vdev->eventfd_count, retval);

            vdev->eventfd_count++;
            virtio_queue_guest_notifier_read(e);
            pthread_mutex_unlock(&vdev->interrupt_lock);
        }
    }
}

EventNotifier *virtio_queue_get_guest_notifier(VirtQueue *vq)
{
    return &vq->guest_notifier;
}

int virtio_loopback_set_guest_notifiers(VirtIODevice *vdev, int nvqs,
                                           bool assign)
{
    bool with_irqfd = false;
    pthread_t thread_id;
    int r, n;

    (void)assign;
    nvqs = MIN(nvqs, VIRTIO_QUEUE_MAX);

    for (n = 0; n < nvqs; n++) {
    
        if (!virtio_queue_get_num(vdev, n)) {
            break;
        }

        VirtQueue *vq = virtio_get_queue(vdev, n);
        EventNotifier *notifier = virtio_queue_get_guest_notifier(vq);
        int r = event_notifier_init(notifier, 0);
        if (r < 0) {
            goto assign_error;
        }

        if (vq->guest_notifier.wfd > 0) {
            r = pthread_create(&thread_id, NULL, loopback_event_select,
                                 (void *)&vq->guest_notifier);
            if (r != 0) {
                goto assign_error;
            }
        }
    }

    return 0;

assign_error:
    DBG("Error return virtio_loopback_set_guest_notifiers\n");
    return r;
}

EventNotifier *virtio_queue_get_host_notifier(VirtQueue *vq)
{
    return &vq->host_notifier;
}

void virtio_queue_set_host_notifier_enabled(VirtQueue *vq, bool enabled)
{
    vq->host_notifier_enabled = enabled;
}

int virtio_bus_set_host_notifier(VirtioBus *vbus, int n, bool assign)
{
    VirtIODevice *vdev = vbus->vdev;
    struct adapter_dev *adev = to_adapter_device(vdev, virtio_dev);
    VirtQueue *vq = virtio_get_queue(vdev, n);
    EventNotifier *notifier = virtio_queue_get_host_notifier(vq);
    int r = 0;

    if (assign) {
        r = event_notifier_init(notifier, 1);
        if (r < 0) {
            DBG("unable to init event notifier: %d", r);
            return r;
        }
    }
    virtio_queue_set_host_notifier_enabled(vq, assign);

    return r;
}

void virtio_queue_notify(VirtIODevice *vdev, int n)
{
    VirtQueue *vq = &vdev->vq[n];

    DBG("virtio_queue_notify(..., vq_n: %d)\n", n);

    if (!vq->vring.desc || vdev->broken) {
        DBG("virtio_queue_notify: broken\n");
        return;
    }

    if (vq->host_notifier_enabled) {
        event_notifier_set(&vq->host_notifier);
    } else if (vq->handle_output) {
        vq->handle_output(vdev, vq);

        if (vdev->start_on_kick) {
            virtio_set_started(vdev, true);
        }
    }
}

uint32_t virtio_config_readb(VirtIODevice *vdev, uint32_t addr)
{
    VirtioDeviceClass *k = vdev->vdev_class;
    uint8_t val;

    if (addr + sizeof(val) > vdev->config_len) {
        DBG("virtio_config_readb failed\n");
        return (uint32_t)-1;
    }

    k->get_config(vdev, vdev->config);

    memcpy(&val, (uint8_t *)(vdev->config + addr), sizeof(uint8_t));

    return val;
}

uint32_t virtio_config_readw(VirtIODevice *vdev, uint32_t addr)
{
    VirtioDeviceClass *k = vdev->vdev_class;
    uint16_t val;

    if (addr + sizeof(val) > vdev->config_len) {
        DBG("virtio_config_readw failed\n");
        return (uint32_t)-1;
    }

    k->get_config(vdev, vdev->config);

    memcpy(&val, (uint16_t *)(vdev->config + addr), sizeof(uint16_t));
    return val;
}

uint32_t virtio_config_readl(VirtIODevice *vdev, uint32_t addr)
{
    VirtioDeviceClass *k = vdev->vdev_class;
    uint32_t val;

    if (addr + sizeof(val) > vdev->config_len) {
        DBG("virtio_config_readl failed\n");
        return (uint32_t)-1;
    }

    k->get_config(vdev, vdev->config);

    memcpy(&val, (uint32_t *)(vdev->config + addr), sizeof(uint32_t));
    return val;
}

void virtio_config_writeb(VirtIODevice *vdev, uint32_t addr, uint32_t data)
{
    VirtioDeviceClass *k = vdev->vdev_class;
    uint8_t val = data;

    if (addr + sizeof(val) > vdev->config_len) {
        return;
    }

    memcpy((uint8_t *)(vdev->config + addr), &val, sizeof(uint8_t));

    if (k->set_config) {
        k->set_config(vdev, vdev->config);
    }
}

void virtio_config_writew(VirtIODevice *vdev, uint32_t addr, uint32_t data)
{
    VirtioDeviceClass *k = vdev->vdev_class;
    uint16_t val = data;

    if (addr + sizeof(val) > vdev->config_len) {
        return;
    }

    memcpy((uint16_t *)(vdev->config + addr), &val, sizeof(uint16_t));

    if (k->set_config) {
        k->set_config(vdev, vdev->config);
    }
}

void virtio_config_writel(VirtIODevice *vdev, uint32_t addr, uint32_t data)
{
    VirtioDeviceClass *k = vdev->vdev_class;
    uint32_t val = data;

    if (addr + sizeof(val) > vdev->config_len) {
        return;
    }

    memcpy((uint32_t *)(vdev->config + addr), &val, sizeof(uint32_t));

    if (k->set_config) {
        k->set_config(vdev, vdev->config);
    }
}

static uint64_t virtio_loopback_read(VirtIODevice *vdev, uint64_t offset,
                                 unsigned size)
{

    uint64_t ret;
    struct adapter_dev *adev = to_adapter_device(vdev, virtio_dev);

    if (!vdev) {
        /*
         * If no backend is present, we treat most registers as
         * read-as-zero, except for the magic number, version and
         * vendor ID. This is not strictly sanctioned by the virtio
         * spec, but it allows us to provide transports with no backend
         * plugged in which don't confuse Linux's virtio code: the
         * probe won't complain about the bad magic number, but the
         * device ID of zero means no backend will claim it.
         */
        switch (offset) {
        case VIRTIO_MMIO_MAGIC_VALUE:
            return VIRT_MAGIC;
        case VIRTIO_MMIO_VERSION:
            if (adev->proxy.legacy) {
                return VIRT_VERSION_LEGACY;
            } else {
                return VIRT_VERSION;
            }
        case VIRTIO_MMIO_VENDOR_ID:
            return VIRT_VENDOR;
        default:
            return 0;
        }
    }

    if (offset >= VIRTIO_MMIO_CONFIG) {
        offset -= VIRTIO_MMIO_CONFIG;

        if (adev->proxy.legacy) {
            switch (size) {
            case 1:
                ret = virtio_config_readb(vdev, offset);
                break;
            case 2:
                ret = virtio_config_readw(vdev, offset);
                break;
            case 4:
                ret = virtio_config_readl(vdev, offset);
                break;
            default:
                DBG("VIRTIO_MMIO_CONFIG abort\n");
                abort();
            }
            return ret;
        }

        return 4;
    }

    if (size != 4) {
        DBG("wrong size access to register!\n");
        return 0;
    }

    switch (offset) {
    case VIRTIO_MMIO_MAGIC_VALUE:
        return VIRT_MAGIC;
    case VIRTIO_MMIO_VERSION:
        if (adev->proxy.legacy) {
            DBG("VIRTIO_MMIO_VERSION -> legacy\n");
            return VIRT_VERSION_LEGACY;
        } else {
            DBG("VIRTIO_MMIO_VERSION -> version\n");
            return VIRT_VERSION;
        }
    case VIRTIO_MMIO_DEVICE_ID:
        return vdev->device_id;
    case VIRTIO_MMIO_VENDOR_ID:
        return VIRT_VENDOR;
    case VIRTIO_MMIO_DEVICE_FEATURES:
        if (adev->proxy.legacy) {
            if (adev->proxy.host_features_sel) {
                return vdev->host_features >> 32;
            } else {
                return vdev->host_features & (uint64_t)(((1ULL << 32) - 1));
            }
        } else {
            /* TODO: To be implemented */
            return 0;
        }
    case VIRTIO_MMIO_QUEUE_NUM_MAX:
        DBG("Return queue_size = %d\n", vdev->vq[vdev->queue_sel].vring.num);
        return vdev->vq[vdev->queue_sel].vring.num;
    case VIRTIO_MMIO_QUEUE_PFN:
        if (!adev->proxy.legacy) {
            DBG("VIRTIO_MMIO_QUEUE_PFN: read from legacy register (0x%lx) "
                "in non-legacy mode\n", offset);
            return 0;
        }
        return virtio_queue_get_addr(vdev, vdev->queue_sel) >>
                                            adev->proxy.guest_page_shift;

    case VIRTIO_MMIO_QUEUE_READY:
        if (adev->proxy.legacy) {
            DBG("VIRTIO_MMIO_QUEUE_READY: read from legacy register (0x%lx) "
                "in non-legacy mode\n", offset);
            return 0;
        }
        /* TODO: To be implemented */
        DBG("VIRTIO_MMIO_QUEUE_READY: Not implemented case\n");
        return 0;
    case VIRTIO_MMIO_INTERRUPT_STATUS:
        int temp;
        pthread_mutex_lock(&vdev->isr_lock);
        temp = vdev->isr;
        pthread_mutex_unlock(&vdev->isr_lock);
        return temp;
    case VIRTIO_MMIO_STATUS:
        DBG("Read VIRTIO_MMIO_STATUS: %d\n", vdev->status);
        return vdev->status;
    case VIRTIO_MMIO_CONFIG_GENERATION:
        if (adev->proxy.legacy) {
            DBG("VIRTIO_MMIO_CONFIG_GENERATION: read from legacy "
                "register (0x%lx) in non-legacy mode\n", offset);
            return 0;
        }
        return vdev->generation;
    case VIRTIO_MMIO_SHM_LEN_LOW:
    case VIRTIO_MMIO_SHM_LEN_HIGH:
        /*
         * VIRTIO_MMIO_SHM_SEL is unimplemented
         * according to the linux driver, if region length is -1
         * the shared memory doesn't exist
         */
        return -1;
    case VIRTIO_MMIO_DEVICE_FEATURES_SEL:
    case VIRTIO_MMIO_DRIVER_FEATURES:
    case VIRTIO_MMIO_DRIVER_FEATURES_SEL:
    case VIRTIO_MMIO_GUEST_PAGE_SIZE:
    case VIRTIO_MMIO_QUEUE_SEL:
    case VIRTIO_MMIO_QUEUE_NUM:
    case VIRTIO_MMIO_QUEUE_ALIGN:
    case VIRTIO_MMIO_QUEUE_NOTIFY:
    case VIRTIO_MMIO_INTERRUPT_ACK:
    case VIRTIO_MMIO_QUEUE_DESC_LOW:
    case VIRTIO_MMIO_QUEUE_DESC_HIGH:
    case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
    case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
    case VIRTIO_MMIO_QUEUE_USED_LOW:
    case VIRTIO_MMIO_QUEUE_USED_HIGH:
        DBG("VIRTIO_MMIO_QUEUE_USED_HIGH: read of write-only "
            "register (0x%lx)\n", offset);
        return 0;
    default:
        DBG("read: bad register offset (0x%lx)\n", offset);
        return 0;
    }
    return 0;
}

void virtio_loopback_write(VirtIODevice *vdev, uint64_t offset,
                       uint64_t value, unsigned size)
{
    struct adapter_dev *adev = to_adapter_device(vdev, virtio_dev);

    if (!vdev) {
        /*
         * If no backend is present, we just make all registers
         * write-ignored. This allows us to provide transports with
         * no backend plugged in.
         */
        return;
    }

    if (offset >= VIRTIO_MMIO_CONFIG) {
        offset -= VIRTIO_MMIO_CONFIG;

        if (adev->proxy.legacy) {
            switch (size) {
            case 1:
                virtio_config_writeb(vdev, offset, value);
                break;
            case 2:
                virtio_config_writew(vdev, offset, value);
                break;
            case 4:
                virtio_config_writel(vdev, offset, value);
                break;
            default:
                DBG("VIRTIO_MMIO_CONFIG abort\n");
                abort();
            }
            return;
        }

        return;
    }
    if (size != 4) {
        DBG("write: wrong size access to register!\n");
        return;
    }
    switch (offset) {
    case VIRTIO_MMIO_DEVICE_FEATURES_SEL:
        DBG("VIRTIO_MMIO_DEVICE_FEATURES_SEL: 0x%lx\n", value);
        if (value) {
            adev->proxy.host_features_sel = 1;
        } else {
            adev->proxy.host_features_sel = 0;
        }
        break;
    case VIRTIO_MMIO_DRIVER_FEATURES:
        if (adev->proxy.legacy) {
            if (adev->proxy.guest_features_sel) {
                DBG("Set driver features: 0x%lx\n", value << 32);
                virtio_set_features(vdev, value << 32);
            } else {
                DBG("Set driver features: 0x%lx\n", value);
                virtio_set_features(vdev, value);
            }
        } else {
            /* TODO: To be implemented */
        }
        break;
    case VIRTIO_MMIO_DRIVER_FEATURES_SEL:
        if (value) {
            adev->proxy.guest_features_sel = 1;
        } else {
            adev->proxy.guest_features_sel = 0;
        }
        break;
    case VIRTIO_MMIO_GUEST_PAGE_SIZE:
        if (!adev->proxy.legacy) {
            DBG("write to legacy register (0x%lx"
                    ") in non-legacy mode\n", offset);
            return;
        }
        if (adev->proxy.guest_page_shift > 31) {
            adev->proxy.guest_page_shift = 0;
        }
        break;
    case VIRTIO_MMIO_QUEUE_SEL:
        if (value < VIRTIO_QUEUE_MAX) {
            vdev->queue_sel = value;
        }
        break;
    case VIRTIO_MMIO_QUEUE_NUM:
        DBG("VIRTIO_MMIO_QUEUE_NUM: %lu\n", value);

        virtio_queue_set_num(vdev, vdev->queue_sel, value);

        if (adev->proxy.legacy) {
            virtio_queue_update_rings(vdev, vdev->queue_sel);
        } else {
            /* TODO: To be implemented */
            exit(1);
        }
        break;
    case VIRTIO_MMIO_QUEUE_ALIGN:
        if (!adev->proxy.legacy) {
            DBG("write to legacy register (0x%lx) in "
                "non-legacy mode\n", offset);
            return;
        }
        /* TODO: To be implemented */
        break;
    case VIRTIO_MMIO_QUEUE_PFN:
        if (!adev->proxy.legacy) {
            DBG("write to legacy register (0x%lx) in "
                "non-legacy mode\n", offset);
            return;
        }
        if (value == 0) {
            /* TODO: To be implemented */
            DBG("Not implemented\n");
        } else {

            DBG("desc_addr: 0x%lx\n", value);
            vdev->vring_phys_addrs[vdev->vring_phys_addrs_idx++] = value;

            uint64_t desc_addr;
            uint32_t vqs_size = get_vqs_max_size(vdev);

            ioctl(adev->loopback_fd, SHARE_VQS, &vdev->queue_sel);

            desc_addr = (uint64_t)mmap(NULL, vqs_size,
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED, adev->loopback_fd, 0);

            virtio_queue_set_addr(vdev, vdev->queue_sel,
                                  desc_addr);
        }
        break;
    case VIRTIO_MMIO_QUEUE_READY:
        if (adev->proxy.legacy) {
            DBG("write to non-legacy register (0x%lx) in "
                "legacy mode\n", offset);
            return;
        }
        /* TODO: To be implemented */
        break;
    case VIRTIO_MMIO_QUEUE_NOTIFY:
        DBG("VIRTIO_MMIO_QUEUE_NOTIFY: vq_index -> %lu, notify_cnt: %d\n",
            value, vdev->notify_cnt++);
        if (value < VIRTIO_QUEUE_MAX) {
            virtio_queue_notify(vdev, value);
        }
        break;
    case VIRTIO_MMIO_INTERRUPT_ACK:
        pthread_mutex_lock(&vdev->isr_lock);
        vdev->isr = vdev->isr & ~value;
        pthread_mutex_unlock(&vdev->isr_lock);
        virtio_notify_vector(vdev);
        break;
    case VIRTIO_MMIO_STATUS:

        /*
         * TODO: Add it in a future release later
         *
         * if (!(value & VIRTIO_CONFIG_S_DRIVER_OK)) {
         *     virtio_loopback_stop_ioeventfd(adev->proxy);
         * }
         */

        if (!adev->proxy.legacy && (value & VIRTIO_CONFIG_S_FEATURES_OK)) {
            virtio_set_features(vdev,
                                ((uint64_t)adev->proxy.guest_features[1]) << 32 |
                                adev->proxy.guest_features[0]);
        }

        virtio_set_status(vdev, value & 0xff);

        DBG("STATUS -> %lu\n", value);

        /*
         * TODO: Check if this is still needed
         *
         * if (vdev->status == 0) {
         *     virtio_reset(vdev);
         *     virtio_loopback_soft_reset(adev->proxy);
         * }
         */

        break;
    case VIRTIO_MMIO_QUEUE_DESC_LOW:
        if (adev->proxy.legacy) {
            DBG("write to non-legacy register (0x%lx) in "
                "legacy mode\n", offset);
            return;
        }
        /* TODO: To be implemented */
        break;
    case VIRTIO_MMIO_QUEUE_DESC_HIGH:
        if (adev->proxy.legacy) {
            DBG("write to non-legacy register (0x%lx) in "
                "legacy mode\n", offset);
            return;
        }
        /* TODO: To be implemented */
        break;
    case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
        if (adev->proxy.legacy) {
            DBG("write to non-legacy register (0x%lx) in "
                "legacy mode\n", offset);
            return;
        }
        /* TODO: To be implemented */
        break;
    case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
        if (adev->proxy.legacy) {
            DBG("write to non-legacy register (0x%lx) in "
                "legacy mode\n", offset);
            return;
        }
        /* TODO: To be implemented */
        break;
    case VIRTIO_MMIO_QUEUE_USED_LOW:
        if (adev->proxy.legacy) {
            DBG("write to non-legacy register (0x%lx) in "
                "legacy mode\n", offset);
            return;
        }
        /* TODO: To be implemented */
        break;
    case VIRTIO_MMIO_QUEUE_USED_HIGH:
        if (adev->proxy.legacy) {
            DBG("write to non-legacy register (0x%lx) in "
                "legacy mode\n", offset);
            return;
        }
        /* TODO: To be implemented */
        break;
    case VIRTIO_MMIO_MAGIC_VALUE:
    case VIRTIO_MMIO_VERSION:
    case VIRTIO_MMIO_DEVICE_ID:
    case VIRTIO_MMIO_VENDOR_ID:
    case VIRTIO_MMIO_DEVICE_FEATURES:
    case VIRTIO_MMIO_QUEUE_NUM_MAX:
    case VIRTIO_MMIO_INTERRUPT_STATUS:
    case VIRTIO_MMIO_CONFIG_GENERATION:
        /* TODO: To be implemented */
        break;
    default:
        DBG("bad register offset (0x%lx)\n", offset);
    }
}

void adapter_read_write_cb(struct adapter_dev *adev)
{
    /*
     * Enabling the next line, all the incoming
     * read/write events will be printed:
     *
     * print_neg_flag (adev->address->notification, adev->address->read);
     */
    print_neg_flag(adev->address->notification, adev->address->read);

    if (adev->address->read) {
        adev->address->data = virtio_loopback_read(&adev->virtio_dev,
                                         adev->address->notification, adev->address->size);
    } else {
        virtio_loopback_write(&adev->virtio_dev, adev->address->notification,
                                       adev->address->data, adev->address->size);
    }

    DBG("Return to the driver\n");

    /*
     * Note the driver that we have done
     * all the required actions.
     */
    (void)ioctl(adev->loopback_fd, WAKEUP);

}

void *driver_event_select(void *data)
{
    int retval, s;
    uint64_t eftd_ctr;
    fd_set rfds;
    struct adapter_dev *adev = (struct adapter_dev *)data;
    int efd = adev->efd;

    DBG("\nWaiting for loopback read/write events (efd :%d)\n", efd);

    FD_ZERO(&rfds);
    FD_SET(efd, &rfds);

    while (1) {

        retval = select(efd + 1, &rfds, NULL, NULL, NULL);

        if (retval == -1) {
            DBG("\nselect() error. Exiting...");
            exit(EXIT_FAILURE);
        } else if (retval > 0) {

            s = read(efd, &eftd_ctr, sizeof(uint64_t));
            if (s != sizeof(uint64_t)) {
                DBG("\neventfd read error. Exiting...");
                exit(1);
            } else {
                adapter_read_write_cb(adev);
            }

        } else if (retval == 0) {
            DBG("\nselect() says that no data was available");
        }
    }
}

VirtQueue *virtio_get_queue(VirtIODevice *vdev, int n)
{
    return vdev->vq + n;
}

VirtQueue *virtio_add_queue(VirtIODevice *vdev, int queue_size,
                            VirtIOHandleOutput handle_output)
{
    int i;

    for (i = 0; i < VIRTIO_QUEUE_MAX; i++) {
        if (vdev->vq[i].vring.num == 0) {
            break;
        }
    }

    if (i == VIRTIO_QUEUE_MAX || queue_size > VIRTQUEUE_MAX_SIZE) {
        DBG("Error: queue_size > VIRTQUEUE_MAX_SIZE\n");
        exit(1);
    }

    vdev->vq[i].vring.num = queue_size;
    vdev->vq[i].vring.num_default = queue_size;
    vdev->vq[i].vring.align = VIRTIO_PCI_VRING_ALIGN;
    vdev->vq[i].handle_output = handle_output;
    vdev->vq[i].used_elems = (VirtQueueElement *)malloc(sizeof(VirtQueueElement)
                                                        * queue_size);

    return &vdev->vq[i];
}

void virtio_dev_init(VirtIODevice *vdev, const char *name,
                 uint16_t device_id, size_t config_size)
{
    struct adapter_dev *adev = to_adapter_device(vdev, virtio_dev);
    int i;

    DBG("virtio_dev_init\n");

    /* Initialize global variables */
    vdev->prev_level = 0;
    vdev->int_count = 0;
    vdev->enable_virtio_interrupt = false;
    vdev->vring_phys_addrs_idx = 0;
    vdev->eventfd_count = 0;
    vdev->notify_cnt = 0;

    /* Initialize interrupt mutex */
    if (pthread_mutex_init(&vdev->interrupt_lock, NULL) != 0) {
        printf("[ERROR] mutex init has failed\n");
        exit(1);
    }

    if (pthread_mutex_init(&vdev->isr_lock, NULL) != 0) {
        printf("[ERROR] mutex init has failed\n");
        exit(1);
    }

    vdev->start_on_kick = false;
    vdev->started = false;
    vdev->device_id = device_id;
    vdev->status = 0;
    vdev->last_avail = -1;
    vdev->queue_sel = 0;
    vdev->config_vector = VIRTIO_NO_VECTOR;
    /* TODO: check malloc return value */
    vdev->vq = (VirtQueue *) malloc(sizeof(VirtQueue) * VIRTIO_QUEUE_MAX);
    vdev->vm_running = false;
    vdev->broken = false;
    for (i = 0; i < VIRTIO_QUEUE_MAX; i++) {
        vdev->vq[i].vector = VIRTIO_NO_VECTOR;
        vdev->vq[i].vdev = vdev;
        vdev->vq[i].queue_index = i;
        vdev->vq[i].host_notifier_enabled = false;
    }

    vdev->name = name;
    vdev->config_len = config_size;
    if (vdev->config_len) {
        vdev->config = (void *) malloc(config_size);
    } else {
        vdev->config = NULL;
    }

    /* Initialize device_info */
    adev->device_info.magic = 0x74726976;
    adev->device_info.version = 0x1;
    adev->device_info.device_id = device_id;
    adev->device_info.vendor = 0x554d4551;
    adev->device_info.priority_enabled = adev->device_params->priority_enabled;
    adev->device_info.credits = (unsigned long)adev->device_params->credits;
    adev->device_info.priority_group = (unsigned long)adev->device_params->priority_group;

    vdev->use_guest_notifier_mask = true;
}

static bool virtio_loopback_ioeventfd_enabled(VirtIODevice *d)
{
    struct adapter_dev *adev = to_adapter_device(d, virtio_dev);

    return (adev->proxy.flags & VIRTIO_IOMMIO_FLAG_USE_IOEVENTFD) != 0;
}

bool virtio_bus_device_iommu_enabled(VirtIODevice *vdev)
{
    VirtioBus *k = vdev->vbus;

    if (!k->iommu_enabled) {
        return false;
    }

    return k->iommu_enabled(vdev);
}

void virtio_loopback_bus_init(VirtioBus *k)
{
    DBG("virtio_loopback_bus_init(...)\n");
    k->set_guest_notifiers = virtio_loopback_set_guest_notifiers;
    k->ioeventfd_enabled = virtio_loopback_ioeventfd_enabled;
}

int virtio_loopback_start(struct adapter_dev *adev, pthread_t *thread_id)
{
    efd_data_t info;
    int ret = -1;

    adev->loopback_fd = open("/dev/loopback", O_RDWR);
    if (adev->loopback_fd < 0) {
        perror("Open call failed");
        return -1;
    }

    /* Create eventfd */
    adev->efd = eventfd(0, 0);
    if (adev->efd == -1) {
        DBG("\nUnable to create eventfd! Exiting...\n");
        exit(EXIT_FAILURE);
    }

    info.pid = getpid();
    info.efd[0] = adev->efd;

    /*
     * Send the appropriate information to the driver
     * so to be able to trigger an eventfd
     */
    (void)ioctl(adev->loopback_fd, EFD_INIT, &info);

    /* Map communication mechanism */
    (void)ioctl(adev->loopback_fd, SHARE_COM_STRUCT);
    adev->address = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, adev->loopback_fd, 0);
    if (adev->address == MAP_FAILED) {
        perror("mmap operation failed");
        return -1;
    }

    /* Wait the eventfd */
    ret = pthread_create(thread_id, NULL, driver_event_select, (void *)adev);
    if (ret != 0) {
        goto error_start;
    }

    /* Start loopback transport */
    (void)ioctl(adev->loopback_fd, START_LOOPBACK, &adev->device_info);

error_start:
    return ret;
}

void virtio_loopback_stop(struct adapter_dev *adev)
{
    DBG("\nClosing eventfd\n");
    close(adev->efd);

    DBG("\nClosing loopback fd. Exiting...\n");
    close(adev->loopback_fd);
}
