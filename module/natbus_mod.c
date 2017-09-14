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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/device.h>

MODULE_LICENSE("GPL v2");

#include "natbus.h"

static int natbus_match( struct device *dev, struct device_driver *driver )
{
  if( !dev || !driver )
    return -EINVAL;

  if( strncmp( dev->bus->name, "nat", 3 ) != 0 )
    return -EINVAL;

  if( strncmp( driver->bus->name, "nat", 3 ) != 0 )
    return -EINVAL;

  return driver->probe( dev );
}

struct bus_type natbus_type = {
  .name     = "nat",
  .match    = natbus_match,
};

static void natbus_dev_release( struct device *dev )
{
  if( dev && dev_name(dev) )
    printk( KERN_DEBUG "NAT: releasing device (%s)\n", dev_name(dev) );
  else
    printk( KERN_ERR "%s(): Invalid release call\n", __FUNCTION__ );
}

struct device natbus_dev = {
  //.bus_id   = "0.0.0.0",
  .release  = natbus_dev_release
};

int register_nat_device( struct nat_device *natdev )
{
  if( !natdev )
    return -EINVAL;

  natdev->device.bus = &natbus_type;
  natdev->device.parent = &natbus_dev;
  natdev->device.release = natbus_dev_release;
  dev_set_name(&natdev->device, natdev->name);
  //strncpy( natdev->device.bus_id, natdev->name, NATBUS_ID_SIZE );
  //strncpy( dev_name( &natdev->device ), natdev->name, NATBUS_ID_SIZE );

  printk( KERN_DEBUG "NAT: registering device (%s)\n", dev_name(&natdev->device) );

  return device_register( &natdev->device );
}
EXPORT_SYMBOL( register_nat_device );

int register_nat_driver( struct nat_driver *natdrv )
{
  if( !natdrv )
    return -EINVAL;

  natdrv->driver.bus = &natbus_type;
  return driver_register( &natdrv->driver );
}
EXPORT_SYMBOL( register_nat_driver );

static int __init natbus_module_init( void )
{
  int ret;

  printk( KERN_DEBUG "Initializing nat bus\n" );

  if( (ret = bus_register( &natbus_type )) )
  {
    printk( KERN_ERR "%s(): Failed to register nat bus\n", __FUNCTION__ );
    return ret;
  }

  dev_set_name( &natbus_dev, "0.0.0.0" );
  if( (ret = device_register( &natbus_dev )) )
  {
    printk( KERN_ERR "%s(): Failed to register the nat bus root device (%d)\n", __FUNCTION__, ret );
    bus_unregister( &natbus_type );
    return ret;
  }

  return 0;
}

static void __exit natbus_module_exit( void )
{
  device_unregister( &natbus_dev );
  bus_unregister( &natbus_type );
}

module_init( natbus_module_init );
module_exit( natbus_module_exit );

MODULE_AUTHOR( "Michael Beckers" );
