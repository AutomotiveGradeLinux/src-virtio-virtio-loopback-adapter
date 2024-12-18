/*
 * Copyright 2022-2024 Virtual Open Systems SAS
 *
 * Authors:
 *  Timos Ampelikiotis <t.ampelikiotis@virtualopensystems.com>
 *  Stefanos Gerangelos <s.gerangelos@virtualopensystems.com>
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
#include <sys/eventfd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/param.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

/* For socket */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

/* Project header files */
#include "adapter.h"

#ifdef DEBUG
#define DBG(...) printf("adapter: " __VA_ARGS__)
#else
#define DBG(...)
#endif /* DEBUG */

/* Global variables */
struct adapter_dev* adevs[MAX_DEVICES];
Device devices[MAX_DEVICES];

static int alloc_adapter_dev(struct adapter_dev **adevs_ptr, Device *device_params)
{
    struct adapter_dev *adev;

    DBG("Setup adapter data structures\n");

    /* Init adapter device */
    adev = (struct adapter_dev *)malloc(sizeof(struct adapter_dev));
    adev->device_params = device_params;

    /* Add device to global structure */
    *adevs_ptr = adev;

    /* Interconnections */
    adev->vbus.vdev = &adev->virtio_dev;
    adev->virtio_dev.vbus = &adev->vbus;
    adev->virtio_dev.vhdev = &adev->vdev;
    adev->vdev.vdev = &adev->virtio_dev;
    adev->vudev.dev = &adev->vdev;
    adev->vdev.vhudev = &adev->vudev;

    return 0;
}

static int init_adapter_data_structure(int active_devices)
{
    int i, err;

    for (i = 0; i < active_devices; i++) {
        err = alloc_adapter_dev(&adevs[i], &devices[i]);
        if (err)
            return -1;
    }

    return 0;
}

static void client(Device *device)
{
    int rc, len;
    struct sockaddr_un client_sockaddr;
    char *sock_path = device->socket;

    DBG("Create shared socket with vhost-user-device\n");

    /* Initialize the struct to zero */
    memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));

    /*
     * Create a UNIX socket
     */
    device->client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (device->client_socket == -1) {
        DBG("SOCKET ERROR\n");
        exit(1);
    }

    /*
     * Set up the UNIX sockaddr structure
     * by using AF_UNIX for the family and
     * giving it a filepath to connect.
     */
    client_sockaddr.sun_family = AF_UNIX;
    strcpy(client_sockaddr.sun_path, sock_path);
    len = sizeof(client_sockaddr);
    rc = connect(device->client_socket, (struct sockaddr *) &client_sockaddr, len);
    if (rc == -1) {
        printf("CONNECT ERROR: Check the \"socket\" parameter: %s\n", sock_path);
        close(device->client_socket);
        exit(1);
    }
}

int main(int argc, char **argv)
{
    int device_count, i, ret;
    pthread_t thread_ids[MAX_DEVICES];


    /* Parse arguments */
    device_count = parse_args(argc, argv);
    if (device_count < 0)
        goto error_args;

    /* Initialize the adapter data structures */
    init_adapter_data_structure(device_count);

    for (i = 0; i < device_count; i++) {

        /*
         * Create the socket and connect to the backend.
         * Enabled on vhost-user case
         */
        if (devices[i].requires_socket) {
            client(&devices[i]);
        }

        /* Initialize the virtio/vhost-user device */
        /* TODO: Switch numbers with name defs */
        switch (devices[i].device_index) {
        case 0:
            virtio_rng_realize(adevs[i]);
            break;
        case 1:
            vhost_user_rng_realize(adevs[i]);
            break;
        case 2:
            vhost_user_blk_realize(adevs[i]);
            break;
        case 3:
            virtio_input_device_realize(adevs[i]);
            break;
        case 4:
            vus_device_realize(adevs[i]);
            break;
        case 5:
            vu_gpio_device_realize(adevs[i]);
            break;
        case 6:
            vhost_user_can_realize(adevs[i]);
            break;
        case 7:
            vhost_user_console_realize(adevs[i]);
            break;
        default:
            exit(1);
        }

        /*
         * Start loopback trasnport layer and communiation with the loopback driver
         */
        DBG("Start loobpack[%i]\n", i);
        ret = virtio_loopback_start(adevs[i], &thread_ids[i]);
        if (ret != 0) {
            DBG("Error starting device: %d\n", i);
            goto error_driver;
        }
    }

    for (i = 0; i < device_count; i++) {
        ret = pthread_join(thread_ids[i], NULL);
        if (ret != 0) {
            DBG("Virtio thread: %d terminated with error\n", (int)thread_ids[i]);
            exit(1);
        }
        /* Close open file descriptor */
        virtio_loopback_stop(adevs[i]);
    }

    return 0;

error_args:
    help_args();
error_driver:
    return 1;
}
