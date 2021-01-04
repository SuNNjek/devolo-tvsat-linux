/* SPDX-License-Identifier: GPL-2.0-or-later */
//////////////////////////////////////////////////////////////////////////
// NAT bus - a virtual bus for Network Attached Tuner devices
// Copyright (C) 2008 devolo AG. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// Contact information:
//    devolo AG
//    Sonnenweg 11
//    D-52070 Aachen, Germany
//    gpl@devolo.de
//////////////////////////////////////////////////////////////////////////
#ifndef __NATBUS_H
#define __NATBUS_H

#include <linux/device.h>

#define NATBUS_ID_SIZE 20

struct nat_driver {
	struct device_driver      driver;
	struct module            *module;
	char                     *version;
};

struct nat_device {
	struct device       device;
	struct nat_driver  *driver;
	char               *name;
};

extern struct bus_type natbus_type;

int register_nat_device(struct nat_device *natdev);
int register_nat_driver(struct nat_driver *natdrv);

#endif
