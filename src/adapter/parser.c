/*
 * Copyright 2022-2024 Virtual Open Systems SAS
 *
 * Authors:
 *  Timos Ampelikiotis <t.ampelikiotis@virtualopensystems.com>
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/param.h>
#include <ctype.h>
#include <sys/stat.h>

/* For socket */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

/* Project header files */
#include "parser.h"

// Function to check if a string contains only digits
static int is_number(const char *str) {
    if (str == NULL || *str == '\0') return 0;  // Empty string or NULL is invalid
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) return 0;
    }
    return 1;
}

// Function to check if queue_num is a valid positive integer
static int check_queue_num(const char *queue_num_str) {
    if (!is_number(queue_num_str)) {
        return 0;
    }
    int queue_num = atoi(queue_num_str);
    return queue_num > 0;  // Check if the queue_num is positive
}

// Function to check if queue_size is a power of 2
static int check_queue_size(const char *queue_size_str) {
    if (!is_number(queue_size_str)) {
        return 0;
    }
    int queue_size = atoi(queue_size_str);
    // Check if the queue_size is a power of 2
    return (queue_size > 0) && ((queue_size & (queue_size - 1)) == 0);
}

static int check_credits(const char *credits_str) {
    if (!is_number(credits_str)) {
        return 0;
    }
    int credits = atoi(credits_str);
    // Check if the queue_size is a power of 2
    return (credits > 0);
}

static int check_priority(const char *priority_str) {
    if (!is_number(priority_str)) {
        return 0;
    }
    int priority = atoi(priority_str);
    // Check if the queue_size is a power of 2
    return (priority > 0);
}

// Function to check if the socket path is valid
static int check_socket(const char *socket_path) {
    if (socket_path == NULL || strlen(socket_path) == 0) {
        return 0;  // Empty path is invalid
    }
    if (strlen(socket_path) >= MAX_SOCKET_LENGTH) {
        return 0;  // Path too long
    }

    struct stat path_stat;
    if (stat(socket_path, &path_stat) != 0) {
        return 0;  // Path does not exist or cannot be accessed
    }

    if (!S_ISSOCK(path_stat.st_mode)) {
        return 0;  // Path is not a socket file
    }

    return 1;
}

// Function to check if the device name is valid and get its index
static int check_device_name(const char *device_name, int *device_index, int *requires_socket) {

    for (unsigned int i = 0; i < num_adapter_devices; i++) {
        if (strcmp(device_name, adapter_devices[i]) == 0) {
            *device_index = i;
            *requires_socket = 0;
            // Check if the device name is also in vhu_devices
            for (unsigned int j = 0; j < num_vhu_devices; j++) {
                if (strcmp(device_name, vhu_devices[j]) == 0) {
                    *requires_socket = 1;
                    return 1;
                }
            }
            return 1;  // Valid device name
        }
    }
    return 0;  // Invalid device name
}

static int parse_device_arg(const char *arg, Device *device) {

    char *token;
    bool socket_provided = false;

    // Initialize with default values
    device->queue_num = DEFAULT_QUEUE_NUM;
    device->queue_size = DEFAULT_QUEUE_SIZE;
    device->credits = 10;
    device->priority_enabled = false;
    device->priority_group = 1;

    // Extract device name
    token = strtok((char *)arg, ",");
    if (token == NULL) {
        return -1;
    }

    // Check if the device name is valid and get the index and socket requirement
    if (!check_device_name(token, &device->device_index, &device->requires_socket)) {
	printf("no device recognised\n");
        return -1;  // Invalid device name
    }
    strncpy(device->device_name, token, MAX_NAME_LENGTH - 1);
    device->device_name[MAX_NAME_LENGTH - 1] = '\0';  // Ensure null termination

    // Extract other parameters
    while ((token = strtok(NULL, ",")) != NULL) {
        if (strncmp(token, "socket=", 7) == 0) {
            const char *socket_path = token + 7; // Skip "socket="
            if (device->requires_socket) {
                if (!check_socket(socket_path)) {
                    fprintf(stderr, "Error: Socket path: %s is invalid\n", socket_path);
                    return -1;  // Invalid socket path
                }
                strncpy(device->socket, socket_path, MAX_SOCKET_LENGTH - 1);
                device->socket[MAX_SOCKET_LENGTH - 1] = '\0';  // Ensure null termination
                socket_provided = true;
            } else {
                // If device does not allow socket, return an error
                fprintf(stderr, "Error: Socket path is not allowed for device: %s\n", device->device_name);
                return -1;
            }
        } else if (strncmp(token, "queue_num=", 10) == 0) {
            const char *queue_num_str = token + 10;  // Skip "queue_num="
            if (!check_queue_num(queue_num_str)) {
                return -1;  // Invalid queue_num
            }
            device->queue_num = atoi(queue_num_str); // Skip "queue_num="
        } else if (strncmp(token, "queue_size=", 11) == 0) {
            const char *queue_size_str = token + 11;  // Skip "queue_size="
            if (!check_queue_size(queue_size_str)) {
                return -1;  // Invalid queue_size
            }
            device->queue_size = atoi(queue_size_str); // Skip "queue_size="
        } else if (strncmp(token, "credits=", 8) == 0) {
            const char *credits_str = token + 8;
            if (!check_credits(credits_str)) {
                return -1;  // Invalid queue_size
            }
            device->credits = atoi(credits_str);
            device->priority_enabled = true;
        } else if (strncmp(token, "priority=", 9) == 0) {
            const char *priority_str = token + 9;
            if (!check_priority(priority_str)) {
                return -1;  // Invalid queue_size
            }
            device->priority_group = atoi(priority_str);
            device->priority_enabled = true;
        } else {
            printf("Error: there is no argument: \"%s\"\n", token);
            return -1;  // Unknown parameter
        }
    }

    /* Overall check */
    if (device->requires_socket & !socket_provided) {
        printf("No socket is provided\n");
        return -1;
    }

    return 0;
}

int parse_args(int argc, char *argv[]) {
    int device_count = 0;

    if (argc == 0) {
        printf("No device available\n");
	return -1;
    }

    // Iterate over the command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-device") == 0) {
            if (i + 1 < argc && device_count < MAX_DEVICES) {
                if (parse_device_arg(argv[i + 1], &devices[device_count]) == 0) {
                    device_count++;
                } else {
                    return -1;
                }
                i++; // Skip next argument as it's already processed
            } else {
                fprintf(stderr, "Error: Missing or too many device arguments.\n");
                return -1;
            }
        }
    }

    // If everything parsed syccessfully, uutput parsed devices
    for (int i = 0; i < device_count; i++) {
        printf("Device %d:\n", i + 1);
        printf("  Device index: %d\n", devices[i].device_index);
        printf("  Name: %s\n", devices[i].device_name);
        printf("  Socket: %s\n", devices[i].socket);
        printf("  Queue Num: %d\n", devices[i].queue_num);
        printf("  Queue Size: %d\n", devices[i].queue_size);
        printf("  Priority: %d\n", devices[i].priority_enabled);
	if (devices[i].priority_enabled) {
            printf("      Credits: %d\n", devices[i].credits);
            printf("      Group: %d\n", devices[i].priority_group);
	}
    }

    return device_count;
}

void help_args(void)
{
    printf("Run example:\n\t./adapter \\\n"
           "\t\t  -device device_name_1,socket=\"/path_to_socket/common.sock[,queue_num=<QUEUE_NUM>,queue_size<QUEUE_SIZE>]\"\n"
           "\t\t  -device device_name_2,socket=\"/path_to_socket/common.sock[,queue_num=<QUEUE_NUM>,queue_size<QUEUE_SIZE>]\"\n"
           "\t\t  ...\n"
           "\t\t  -device device_name,socket=\"/path_to_socket/common.sock[,queue_num=<QUEUE_NUM>,queue_size<QUEUE_SIZE>]\"\n"
           "The 'device_name' can be one of the following:\n"
           "\tvrng, vhurng, vhublk, vhuinput, vhusnd, vhugpio, vhucan, vhuconsole\n"
           "\n"
           "In case of 'vrng' device, the 'socket' argument is not used\n");
}
