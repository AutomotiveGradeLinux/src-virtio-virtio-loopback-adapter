/*
 * Copyright (c) 2022-2024 Virtual Open Systems SAS.
 *
 * Author:
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

#ifndef PARSER_H
#define PARSER_H

/* Project header files */
#include "adapter.h"

// Default values for queue_num and queue_size
#define DEFAULT_QUEUE_NUM 1
#define DEFAULT_QUEUE_SIZE 1024

/* Global variables */
const char *adapter_devices[] = {"vrng", "vhurng", "vhublk",
                                 "vhuinput", "vhusnd", "vhugpio",
                                 "vhucan", "vhuconsole"};
const char *vhu_devices[] = {"vhurng", "vhublk", "vhuinput",
                             "vhusnd", "vhugpio", "vhucan",
                             "vhuconsole"};
const unsigned int num_adapter_devices = sizeof(adapter_devices) / sizeof(adapter_devices[0]);
const unsigned int num_vhu_devices = num_adapter_devices - 1;

#endif /* PARSER_H */
