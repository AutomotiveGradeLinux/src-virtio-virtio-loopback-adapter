/*
 *
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

#ifndef ADAPTER
#define ADAPTER

/* Project header files */
#include "virtio_loopback.h"
#include "vhost_user_loopback.h"
#include "virtio_rng.h"
#include "vhost_user_rng.h"
#include "vhost_user_blk.h"
#include "vhost_user_input.h"
#include "vhost_user_gpio.h"
#include "vhost_user_sound.h"
#include "vhost_user_can.h"
#include "vhost_user_console.h"

#ifdef DEBUG
#define DBG(...) printf("adapter: " __VA_ARGS__)
#else
#define DBG(...)
#endif /* DEBUG */

#define VHU_DEVS 8
#define ADAPTER_DEVS (VHU_DEVS + 1)

const char *adapter_devices[] = {"vrng", "vhurng", "vhublk", "vhuinput",
                                 "vhusnd", "vhugpio", "vhucan",
                                 "vhuconsole" };
const char *vhu_devices[] = {"vhurng", "vhublk", "vhuinput", "vhusnd",
                             "vhugpio", "vhucan", "vhuconsole"};

#endif /* ADAPTER */
