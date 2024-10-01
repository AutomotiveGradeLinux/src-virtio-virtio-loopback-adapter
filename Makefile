# Copyright 2022-2023 Virtual Open Systems SAS
#
# Authors:
#  Timos Ampelikiotis <t.ampelikiotis@virtualopensystems.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.


CFLAGS := -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function

CC ?=

ifeq ($(ARCH), arm64)
 # arm64
 CC ?= aarch64-linux-gnu-gcc
else
    ifeq ($(ARCH), riscv64)
     # arm64
     CC ?= riscv64-linux-gnu-gcc
    else
     CC ?= gcc
    endif
endif

INCL += -I .
INCL += -I ./include/

DEPS = adapter.h vhost_user_loopback.h event_notifier.h virtio_loopback.h

# Source directories
SRC_DIRS = src/adapter src/lib src/common src/lib src/devices

# Source files
SRC_C = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))

# Object files
OBJS = $(SRC_C:.c=.o)

# Binary output
BINS = adapter

ifeq ($(DEBUG), 1)
 CFLAGS += -DDEBUG
endif

all: $(BINS)

# Linking
$(BINS): $(OBJS)
	@echo -e "CC\t$@"
	$(CC) $(CFLAGS) $(INCL) $^ -o $@ -lpthread

# Compilation
%.o: %.c
	@echo -e "CC\t$@"
	$(CC) $(CFLAGS) $(INCL) -c $< -o $@

clean:
	rm -f $(OBJS) *~ $(BINS)

.PHONY: all
