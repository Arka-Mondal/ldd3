# Makefile for the scull char module
#
# Copyright (C) 2024  Arka Mondal
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Comment/uncomment the following line to disable/enable debugging
# DEBUG := y

ifeq ($(DEBUG),y)
	DEBUG_FLAGS := -Og -g -DSCULL_DEBUG
else
	DEBUG_FLAGS := -O2
endif

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
# In kbuild contex
ccflags-y += $(DEBUG_FLAGS)

scull-y := main.o
obj-m := scull.o

# Otherwise we were called directly from the command line;
# invoke the kernel build system
else
# In normal make context
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: default
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

.PHONY: clean
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
