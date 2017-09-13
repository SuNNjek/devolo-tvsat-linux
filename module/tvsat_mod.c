//////////////////////////////////////////////////////////////////////////
// devolo dLAN TV Sat driver
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
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <asm/io.h>

#include "/usr/src/linux/drivers/media/dvb-core/dvbdev.h"
#include "/usr/src/linux/drivers/media/dvb-core/dmxdev.h"
#include "/usr/src/linux/drivers/media/dvb-core/dvb_demux.h"

// #include <dvbdev.h>
// #include <dmxdev.h>
// #include <dvb_demux.h>

#include "../include/tvsat.h"
#include "../include/linux/dvb/frontend.h"
#include "natbus.h"

// driver parameters
#define PRODUCT_NAME          "devolo dLAN TV Sat"
#define DRIVER_NAME           "dlan-tvsat"
#define MAX_DEVS              8

// frontend parameters
#define FEEDNUM               256
#define FILTERNUM             256
#define FREQ_MIN              9250000
#define FREQ_MAX              21750000
#define FREQ_STEP             1000
#define SYM_MIN               22000000
#define SYM_MAX               27500000

MODULE_LICENSE("GPL v2");

// simple linked list structure to store events in
struct tvsat_event_list
{
  struct tvsat_event  *first;
  struct tvsat_event  *last;
  unsigned int         count;
};

// this structure represents a device
// it contains everything that is device specific
struct tvsat_device
{
  struct dvb_adapter             *adapter;
  struct dvb_demux               *demux;
  struct tvsat_dev_id            *dev_id;
  struct nat_device              *device;
  struct dmxdev                  *dmxdev;
  struct tvsat_event_list         events;
  struct dvb_device              *frontend;
  int                             in_use;
  struct cdev                     input_cdev;
  int                             tuned;
  struct tvsat_tuning_parameters  tuning_parameters;
};

// the private data of the driver
struct tvsat
{
  struct cdev                     control_cdev;
  dev_t                           dev_node;
  struct tvsat_device             devices[ MAX_DEVS ];
  struct nat_driver               driver;
  struct class                   *nat_class;
  wait_queue_head_t               pollq;
};

static struct tvsat *tvsat = NULL;

// frontend information
// the caps and some of the other parameters were chosen conservatively
// they may not represent the full set of capabilities of the devices
static struct dvb_frontend_info tvsat_frontend_info = {
  .caps = FE_CAN_QPSK | FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
          FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO,
  .frequency_min = FREQ_MIN,
  .frequency_max = FREQ_MAX,
  .symbol_rate_min = SYM_MIN,
  .symbol_rate_max = SYM_MAX,
  .frequency_stepsize = FREQ_STEP,
  .name = PRODUCT_NAME,
  .type = FE_QPSK
};

// initializes an event list
static void tvsat_init_event_list( struct tvsat_event_list *el )
{
  if( !el )
    return;

  el->first = NULL;
  el->last = NULL;
  el->count = 0;
};

// removes an event from the top of a given event list and returns it
static struct tvsat_event *tvsat_pop_event( struct tvsat_event_list *el )
{
  struct tvsat_event *ret;

  if( !el )
    return NULL;

  if( !el->first )
    return NULL;

  ret = el->first;

  if( el->first == el->last )
  {
    el->first = NULL;
    el->last = NULL;
    el->count = 0;
  }
  else
  {
    el->first = el->first->next;
    --el->count;
  }

  ret->next = NULL;

  return ret;
}

// adds an event to the end of a given event list
static void tvsat_add_event( struct tvsat_event_list *el, struct tvsat_event *ev )
{
  struct tvsat_event *old_ev;

  if( !el || !ev )
    return;

  ev->next = NULL;

  if( !el->last )
  {
    el->first = ev;
    el->last = ev;
    el->count = 1;
  }
  else
  {
    el->last->next = ev;
    el->last = ev;
    ++el->count;
  }

  if( el->count == 100 )
  {
    printk( KERN_ERR "%s(): event list overflow\n", __FUNCTION__ );
    old_ev = tvsat_pop_event( el );
    kfree( old_ev );
  }
}

// add a parameterless event to a given event list
static void tvsat_add_event_noparm( struct tvsat_event_list *el, enum tvsat_event_type t )
{
  struct tvsat_event *ev;

  if( !el )
    return;

  ev = kmalloc( sizeof( struct tvsat_event ), GFP_KERNEL );
  ev->type = t;

  tvsat_add_event( el, ev );
}

// add a connect event to a given event list
static void tvsat_add_connect_event( struct tvsat_event_list *el )
{
  tvsat_add_event_noparm( el, TVSAT_EVENT_CONNECT );
}

// add a disconnect event to a given event list
static void tvsat_add_disconnect_event( struct tvsat_event_list *el )
{
  tvsat_add_event_noparm( el, TVSAT_EVENT_DISCONNECT );
}

// add a pid selection event to a given event list
static void tvsat_add_pid_event( struct tvsat_event_list *el, struct tvsat_pid_selection *pid )
{
  struct tvsat_event *ev;

  if( !el )
    return;

  ev = kmalloc( sizeof( struct tvsat_event ), GFP_KERNEL );
  ev->type = TVSAT_EVENT_PID;
  ev->event.pid = *pid;

  tvsat_add_event( el, ev );
}

// adds a tuning event to a given event list
static void tvsat_add_tune_event( struct tvsat_event_list *el, struct tvsat_tuning_parameters *tune )
{
  struct tvsat_event *ev;
  if( !el || !tune )
    return;

  ev = kmalloc( sizeof( struct tvsat_event ), GFP_KERNEL );
  ev->type = TVSAT_EVENT_TUNE;
  ev->event.tune = *tune;

  tvsat_add_event( el, ev );
}

// handles ioctls on our dvb frontends
static long tvsat_frontend_ioctl( /* struct inode *inode, */ struct file *file, unsigned cmd, unsigned long arg )
{
  fe_status_t status = 0;
  struct dvb_frontend_event __user *event = 0;
  struct dvb_frontend_parameters fe_param;
  struct dvb_diseqc_master_cmd dm_cmd;
  __u32 ber, snr, sst;
  int i;
  struct tvsat_device *dev;

  dev = ((struct dvb_device *)file->private_data)->priv;

  switch( cmd )
  {
    case FE_GET_INFO:
      // returns the frontend info
      return copy_to_user( (void __user *)arg, &tvsat_frontend_info, sizeof( struct dvb_frontend_info ) );

    case FE_DISEQC_RESET_OVERLOAD:
      // not implemented
      return 0;

    case FE_DISEQC_SEND_MASTER_CMD:
      // adds a master command to the tuning parameters
      if( !arg )
        return -EINVAL;

      if( copy_from_user( &dm_cmd, (struct dvb_diseqc_master_cmd __user *)arg, sizeof( struct dvb_diseqc_master_cmd ) ) )
        return -EFAULT;

      for( i = 0; i < TVSAT_MAX_DISEQC_CMDS; ++i )
      {
        if( dev->tuning_parameters.diseqc[ i ].type == 0 )
          break;
      }

      if( i == TVSAT_MAX_DISEQC_CMDS )
        return -EFAULT;

      dev->tuning_parameters.diseqc[ i ].type         = 1;
      dev->tuning_parameters.diseqc[ i ].message_len  = dm_cmd.msg_len;
      memcpy( dev->tuning_parameters.diseqc[ i ].message, dm_cmd.msg, 6 );

      return 0;

    case FE_DISEQC_RECV_SLAVE_REPLY:
      // not implemented
      return 0;

    case FE_DISEQC_SEND_BURST:
      // add a simple burst to the tuning parameters
      for( i = 0; i < TVSAT_MAX_DISEQC_CMDS; ++i )
      {
        if( dev->tuning_parameters.diseqc[ i ].type == 0 )
          break;
      }

      if( i == TVSAT_MAX_DISEQC_CMDS )
        return -EFAULT;

      dev->tuning_parameters.diseqc[ i ].type       = 2;
      dev->tuning_parameters.diseqc[ i ].burst_data = (uint16_t)arg;

      return 0;

    case FE_SET_TONE:
      // sets the tone parameter
      dev->tuning_parameters.band = ( unsigned int )arg;

      return 0;

    case FE_SET_VOLTAGE:
      // sets the voltage parameter
      dev->tuning_parameters.polarization = ( unsigned int )arg;

      return 0;

    case FE_ENABLE_HIGH_LNB_VOLTAGE:
      // not implemented
      return 0;

    case FE_READ_STATUS:
      //TODO: return something more sensible
      //
      // this is not really necessary because all the major dvb programs
      // rely on fe_get_event to get locking information
      status = FE_HAS_SIGNAL | FE_HAS_LOCK | FE_HAS_SYNC | FE_HAS_CARRIER | FE_HAS_VITERBI;

      return copy_to_user( (void  __user *)arg, &status, sizeof( fe_status_t ) );

    case FE_READ_BER:
      //TODO: we always return the optimal values here until the dvb api maintainers
      //      have decided on some standardized format
      ber = 0;

      return copy_to_user( (void __user *)arg, &ber, sizeof( __u32 ) );

    case FE_READ_SIGNAL_STRENGTH:
      //TODO: see above
      sst = 0xffff;

      return copy_to_user( ( void __user *)arg, &sst, sizeof( __u32 ) );

    case FE_READ_SNR:
      //TODO: see above
      snr = 0xffff;

      return copy_to_user( ( void __user *)arg, &snr, sizeof( __u32 ) );

    case FE_READ_UNCORRECTED_BLOCKS:
      //TODO: see above
      return 0;

    case FE_SET_FRONTEND:
      // sets the remaining tuning parameters
      // we assume that this gets called after set_tone and set_voltage
      if( !arg )
        return -EINVAL;

      if( copy_from_user( &fe_param, (struct dvb_frontend_parameters __user *)arg, sizeof( struct dvb_frontend_parameters ) ) )
        return -EFAULT;

      dev->tuning_parameters.fec          = fe_param.u.qpsk.fec_inner;
      dev->tuning_parameters.frequency    = fe_param.frequency;
      dev->tuning_parameters.inversion    = fe_param.inversion;

      // the next three are S2 specific and are not supported by the dvb api v3
      // waiting patiently for v5 to make it into the mainline kernel...
      dev->tuning_parameters.modulation   = 0;
      dev->tuning_parameters.pilot        = 0;
      dev->tuning_parameters.roll_off     = 2;

      dev->tuning_parameters.symbol_rate  = fe_param.u.qpsk.symbol_rate;

      tvsat_add_tune_event( &dev->events, &dev->tuning_parameters );

      for( i = 0; i < TVSAT_MAX_DISEQC_CMDS; ++i )
        dev->tuning_parameters.diseqc[ i ].type = 0;

      return 0;

    case FE_GET_FRONTEND:
      // not implemented
      return 0;

    case FE_GET_EVENT:
      // this is the absolute minimum implementation
      // we only have one event saying that the device is now tuned
      event = ( void __user *)arg;

      if( dev->tuned == 0 )
      {
        if( file->f_flags & O_NONBLOCK )
          return -EAGAIN;
      }

      dev->tuned = 0;

      return tvsat_frontend_ioctl( /* inode, */ file, FE_READ_STATUS, ( unsigned long )&event->status );

    case FE_DISHNETWORK_SEND_LEGACY_CMD:
      // not implemented
      return 0;

    default:
      break;
  }

  return 0;
}

// input is always possible because our frontend doesn't block
static unsigned int tvsat_frontend_poll( struct file *file, struct poll_table_struct *wait )
{
  return ( POLLIN | POLLRDNORM | POLLPRI );
}

// notifies the userspace daemon that some dvb application wants to use the device
static int tvsat_frontend_open( struct inode *inode, struct file *file )
{
  struct dvb_device *dvbdev;
  struct tvsat_device *dev;

  dvbdev = file->private_data;
  dev = dvbdev->priv;

  tvsat_add_connect_event( &dev->events );

  return 0;
}

// notifies the userspace daemon that the device is no longer needed
static int tvsat_frontend_release( struct inode *inode, struct file *file )
{
  struct dvb_device *dvbdev;
  struct tvsat_device *dev;

  dvbdev = file->private_data;
  dev = dvbdev->priv;

  tvsat_add_disconnect_event( &dev->events );

  return dvb_generic_release( inode, file );
}

// the frontends' file ops struct
static struct file_operations tvsat_frontend_file_operations = {
  .owner          = THIS_MODULE,
  .compat_ioctl   = tvsat_frontend_ioctl,
  .poll           = tvsat_frontend_poll,
  .open           = tvsat_frontend_open,
  .release        = tvsat_frontend_release,
};

// the template for our frontends
static struct dvb_device tvsat_frontend_template = {
  .users    = ~0,
  .writers  = 1,
  .readers  = (~0) - 1,
  .fops     = &tvsat_frontend_file_operations
};

// tells the userspace daemon to add a certain pid to the pid filter
static int tvsat_start_feed( struct dvb_demux_feed *feed )
{
  struct tvsat_pid_selection pid_sel;
  struct tvsat_device *dev;

  dev = feed->demux->priv;

  pid_sel.action = PID_START;
  pid_sel.pid = feed->pid;
  pid_sel.type = feed->pes_type;
  tvsat_add_pid_event( &dev->events, &pid_sel );

  return 0;
}

// tells the userspace daemon to remove a certain pid from the pid filter
static int tvsat_stop_feed( struct dvb_demux_feed *feed )
{
  struct tvsat_pid_selection pid_sel;
  struct tvsat_device *dev;

  dev = feed->demux->priv;

  pid_sel.action = PID_STOP;
  pid_sel.pid = feed->pid;
  pid_sel.type = feed->pes_type;
  tvsat_add_pid_event( &dev->events, &pid_sel );

  return 0;
}

// sends the data from userspace directly to the demuxer
static ssize_t tvsat_input_write( struct file *file, const char *buf, size_t count, loff_t *offset )
{
  struct tvsat_device *dev;

  dev = &tvsat->devices[ iminor( /* file->f_dentry->d_inode */ file->f_path.dentry->d_inode ) - 1 ];
  dvb_dmx_swfilter( dev->demux, buf, count );

  return count;
}

// handles ioctls on our input devices
static long tvsat_input_ioctl( /* struct inode *inode, */ struct file *file, unsigned int cmd, unsigned long arg )
{
  struct tvsat_event *ev;
  struct tvsat_device *dev;

  dev = &tvsat->devices[ iminor( /* inode */ file->f_path.dentry->d_inode ) - 1 ];

  switch( cmd )
  {
    case TVS_HAS_LOCK:
      // the userspace daemon reports a signal lock
      // reported only once when the locking state changes
      dev->tuned = 1;

      return 0;
    case TVS_GET_EVENT:
      // requests an event from the event list
      if( !arg )
        return -EFAULT;

      ev = tvsat_pop_event( &dev->events );

      if( !ev )
        return -EFAULT;

      if( copy_to_user( ( void __user * )arg, ev, sizeof( struct tvsat_event ) ) )
        return -EFAULT;

      kfree( ev );

      return 0;
    default:
      return -EINVAL;
  }
}

// the input devs' file ops struct
static struct file_operations tvsat_input_file_operations = {
  .owner          = THIS_MODULE,
  .write          = tvsat_input_write,
  .compat_ioctl          = tvsat_input_ioctl,
};

// registers a new device with the nat bus and the dvb subsystem
static int tvsat_register_device( struct tvsat_dev_id *dev_id )
{
  struct tvsat_device *dev;
  int i, ret;
  dev_t dev_node;
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 26 )
  short pref_nums[ DVB_MAX_ADAPTERS ];
  int j;
#endif

  // find a free device slot
  for( i = 0; i < MAX_DEVS; ++i )
  {
    dev = &tvsat->devices[ i ];

    if( !dev->in_use )
      break;
  }

  if( i == MAX_DEVS )
  {
    printk( KERN_ERR "%s(): No more free device slots\n", __FUNCTION__ ); // increase MAX_DEVS if this happens
    return -ENOMEM;
  }

  // create and register a new nat device
  dev->dev_id = kmalloc( sizeof( struct tvsat_dev_id ), GFP_KERNEL );
  memcpy( dev->dev_id, dev_id, sizeof( struct tvsat_dev_id ) );

  dev->device = kmalloc( sizeof( struct nat_device ), GFP_KERNEL );
  memset( dev->device, 0, sizeof( struct nat_device ) );
  dev->device->name = kmalloc( NATBUS_ID_SIZE, GFP_KERNEL );
  snprintf( dev->device->name, NATBUS_ID_SIZE, "%u.%u.%u.%u",
            dev_id->ip_addr[ 0 ], dev_id->ip_addr[ 1 ], dev_id->ip_addr[ 2 ],
            dev_id->ip_addr[ 3 ] );

  register_nat_device( dev->device );

  // create and register a dvb adapter with one frontend and one demuxer
  dev->adapter = kmalloc( sizeof( struct dvb_adapter ), GFP_KERNEL );
  memset( dev->adapter, 0, sizeof( struct dvb_adapter ) );
  dev->demux = kmalloc( sizeof( struct dvb_demux ), GFP_KERNEL );
  memset( dev->demux, 0, sizeof( struct dvb_demux ) );
  dev->dmxdev = kmalloc( sizeof( struct dmxdev ), GFP_KERNEL );
  memset( dev->dmxdev, 0, sizeof( struct dmxdev ) );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 26 )
  for( j = 1; j < DVB_MAX_ADAPTERS; ++j )
    pref_nums[ j ] = -1;

  pref_nums[ 0 ] = dev_id->adapter;
  ret = dvb_register_adapter( dev->adapter, PRODUCT_NAME, THIS_MODULE, &dev->device->device, pref_nums );

  if( dev_id->adapter != -1 && ret != dev_id->adapter )
    printk( KERN_ERR "%s(): Requested adapter%i but got adapter%i instead\n", __FUNCTION__, dev_id->adapter, ret );

  if( ret < 0 )
#else
  if( dev_id->adapter != -1 )
    printk( KERN_INFO "tvsat: You need at least kernel version 2.6.26 to request specific adapter numbers\n" );

  if( (ret = dvb_register_adapter( dev->adapter, PRODUCT_NAME, THIS_MODULE, &dev->device->device )) < 0 )
#endif
  {
    printk( KERN_ERR "%s(): Failed to initialize dvb adapter\n", __FUNCTION__ );
    goto cleanup;
  }

  printk( KERN_NOTICE "tvsat: Registered device %s as DVB adapter%i\n", dev->device->name, ret );

  dev->demux->priv = dev;
  dev->demux->filternum = 256;
  dev->demux->feednum = 256;
  dev->demux->start_feed = tvsat_start_feed;
  dev->demux->stop_feed = tvsat_stop_feed;
  dev->demux->dmx.capabilities = DMX_TS_FILTERING | DMX_SECTION_FILTERING;

  if( dvb_dmx_init( dev->demux ) < 0 )
  {
    printk( KERN_ERR "%s(): Failed to initialize demuxer\n", __FUNCTION__ );
    dvb_unregister_adapter( dev->adapter );
    goto cleanup;
  }

  dev->dmxdev->filternum = 256;
  dev->dmxdev->demux = &dev->demux->dmx;
  dev->dmxdev->capabilities = 0;

  if( dvb_dmxdev_init( dev->dmxdev, dev->adapter ) < 0 )
  {
    printk( KERN_ERR "%s(): Failed to initialize demux device\n", __FUNCTION__ );
    dvb_dmx_release( dev->demux );
    dvb_unregister_adapter( dev->adapter );
    goto cleanup;
  }

  dvb_register_device( dev->adapter, &dev->frontend, &tvsat_frontend_template, dev, DVB_DEVICE_FRONTEND, 1 );

  // create an input character device
  cdev_init( &dev->input_cdev, &tvsat_input_file_operations );
  tvsat->control_cdev.owner = THIS_MODULE;

  dev_node = MKDEV( MAJOR( tvsat->dev_node ), MINOR( tvsat->dev_node ) + i + 1 );

  if( cdev_add( &dev->input_cdev, dev_node, 1 ) < 0 )
  {
    printk( KERN_ERR "%s(): Failed to initialize input device\n", __FUNCTION__ );
    dvb_unregister_device( dev->frontend );
    dvb_dmxdev_release( dev->dmxdev );
    dvb_dmx_release( dev->demux );
    dvb_unregister_adapter( dev->adapter );
    goto cleanup;
  }

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 27 )
  device_create( tvsat->nat_class, NULL, dev_node, NULL, "tvs%i", i );
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 18 )
  device_create( tvsat->nat_class, NULL, dev_node, "tvs%i", i );
#else
  class_device_create( tvsat->nat_class, NULL, dev_node, NULL, "tvs%i", i );
#endif
#endif

  tvsat_init_event_list( &dev->events );

  dev->in_use = 1;

  // return the device's minor number
  return i + 1;

cleanup:
  kfree( dev->dev_id );
  kfree( dev->device->name );
  kfree( dev->device );
  kfree( dev->adapter );
  kfree( dev->demux );
  kfree( dev->dmxdev );

  return -EFAULT;
}

// unregisters a device from the nat bus and the dvb subsystem
static int tvsat_unregister_device( int dev_num )
{
  struct tvsat_device *dev;

  if( dev_num < 0 || dev_num >= MAX_DEVS )
    return -ENODEV;

  dev = &tvsat->devices[ dev_num ];

  if( !dev->in_use )
    return -ENODEV;

  cdev_del( &dev->input_cdev );
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 18 )
  device_destroy( tvsat->nat_class, MKDEV( MAJOR( tvsat->dev_node ), MINOR( tvsat->dev_node ) + dev_num + 1 ) );
#else
  class_device_destroy( tvsat->nat_class, MKDEV( MAJOR( tvsat->dev_node ), MINOR( tvsat->dev_node ) + dev_num + 1 ) );
#endif

  if( dev->frontend )
    dvb_unregister_device( dev->frontend );

  if( dev->dmxdev )
  {
    dvb_dmxdev_release( dev->dmxdev );
    kfree( dev->dmxdev );
  }

  if( dev->demux )
  {
    dvb_dmx_release( dev->demux );
    kfree( dev->demux );
  }

  if( dev->adapter )
  {
    dvb_unregister_adapter( dev->adapter );
    kfree( dev->adapter );
  }

  if( dev->device )
  {
    device_unregister( &dev->device->device );

    if( dev->device->name )
      kfree( dev->device->name );

    kfree( dev->device );
  }

  if( dev->dev_id )
    kfree( dev->dev_id );

  dev->in_use = 0;

  return 0;
}

// compares two device id structures
static int tvsat_dev_id_cmp( struct tvsat_dev_id *id1, struct tvsat_dev_id *id2 )
{
  if( !id1 || !id2 )
    return 0;

  if( memcmp( id1->ip_addr, id2->ip_addr, 4 ) != 0 )
    return 0;

  if( id1->port != id2->port )
    return 0;

  return 1;
}

// handles ioctls on the control device
static long tvsat_control_ioctl( /* struct inode *inode, */ struct file *file, unsigned int cmd, unsigned long arg )
{
  int found, i, minor;
  struct tvsat_dev_id dev_id;

  switch( cmd )
  {
    case TVS_REGISTER_DEVICE:
      // registers a new device
      if( !arg )
        return -EFAULT;

      if( copy_from_user( &dev_id, (void __user *)arg, sizeof( struct tvsat_dev_id ) ) )
        return -EFAULT;

      minor = 0;

      for( i = 0; i < MAX_DEVS; ++i )
        if( tvsat->devices[ i ].in_use && tvsat_dev_id_cmp( &dev_id, tvsat->devices[ i ].dev_id ) )
        {
          printk( KERN_NOTICE "Device already registered. Re-using dvb adapter\n" );
          minor = i + 1;
        }

      if( minor == 0 )
        if( (minor = tvsat_register_device( &dev_id )) < 0 )
          return -EFAULT;

      if( (minor > 255) || (minor < 0) )
        return -EFAULT;

      dev_id.minor = minor;

      if( copy_to_user( (void __user *)arg, &dev_id, sizeof( struct tvsat_dev_id ) ) )
        return -EFAULT;

      return 0;
    case TVS_UNREGISTER_DEVICE:
      // unregisters a device
      if( !arg )
        return -EFAULT;

      if( copy_from_user( &dev_id, (void __user *)arg, sizeof( struct tvsat_dev_id ) ) )
        return -EFAULT;

      found = 0;

      for( i = 0; i < MAX_DEVS; ++i )
        if( tvsat->devices[ i ].in_use && tvsat_dev_id_cmp( &dev_id, tvsat->devices[ i ].dev_id ) )
        {
          found = 1;
          break;
        }

      if( !found )
        return -ENODEV;

      tvsat_unregister_device( i );

      return 0;
    default:
      return -EINVAL;
  }
}

// the control device's file ops structure
static struct file_operations tvsat_control_file_operations = {
  .owner          = THIS_MODULE,
  .compat_ioctl          = tvsat_control_ioctl,
};

// checks if a device with the same id is already registered
static int tvsat_device_probe( struct device *dev )
{
  int i;

  for( i = 0; i < MAX_DEVS; ++i )
    if( tvsat->devices[ i ].in_use && (strncmp( dev_name(dev), dev_name(&tvsat->devices[ i ].device->device), NATBUS_ID_SIZE ) == 0) )
      return -EINVAL;

  return 0;
}

// initializes the driver's private data and creates the control device
static int __init tvsat_module_init( void )
{
  int i;

  printk( KERN_INFO "Initializing " PRODUCT_NAME " driver\n" );

  // initialize private data
  tvsat = kmalloc( sizeof( struct tvsat ), GFP_KERNEL );
  memset( tvsat, 0, sizeof( struct tvsat ) );

  for( i = 0; i < MAX_DEVS; ++i )
    tvsat->devices[ i ].in_use = 0;

  tvsat->driver.driver.name = DRIVER_NAME;
  tvsat->driver.driver.probe = tvsat_device_probe;
  register_nat_driver( &tvsat->driver );

  // allocate device nodes for the control device and all possible input devices
  if( alloc_chrdev_region( &tvsat->dev_node, 0, MAX_DEVS + 1, DRIVER_NAME ) < 0 )
  {
    printk( KERN_ERR "%s(): Failed to allocate control/input devices\n", __FUNCTION__ );
    driver_unregister( &tvsat->driver.driver );
    kfree( tvsat );

    return -1;
  }

  // create a device class
  tvsat->nat_class = class_create( THIS_MODULE, "natctl" );
  cdev_init( &tvsat->control_cdev, &tvsat_control_file_operations );
  tvsat->control_cdev.owner = THIS_MODULE;

  // create the control device
  if( cdev_add( &tvsat->control_cdev, tvsat->dev_node, 1 ) < 0 )
  {
    printk( KERN_ERR "%s(): Failed to initialize control device\n", __FUNCTION__ );
    unregister_chrdev_region( tvsat->dev_node, MAX_DEVS + 1 );
    driver_unregister( &tvsat->driver.driver );
    kfree( tvsat );

    return -1;
  }

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 27 )
  device_create( tvsat->nat_class, NULL, tvsat->dev_node, NULL, "tvsctl" );
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 18 )
  device_create( tvsat->nat_class, NULL, tvsat->dev_node, "tvsctl" );
#else
  class_device_create( tvsat->nat_class, NULL, tvsat->dev_node, NULL, "tvsctl" );
#endif
#endif

  return 0;
}

// unregisters all remaining TV Sat devices, removes the control device
// and destroys the private data
static void __exit tvsat_module_exit( void )
{
  int i;

  printk( KERN_INFO "Unloading " PRODUCT_NAME " driver\n" );

  if( tvsat )
  {
    for( i = 0; i < MAX_DEVS; ++i )
      tvsat_unregister_device( i );

    cdev_del( &tvsat->control_cdev );
    unregister_chrdev_region( tvsat->dev_node, MAX_DEVS + 1 );
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 18 )
    device_destroy( tvsat->nat_class, tvsat->dev_node );
#else
    class_device_destroy( tvsat->nat_class, tvsat->dev_node );
#endif
    class_destroy( tvsat->nat_class );
    driver_unregister( &tvsat->driver.driver );
    kfree( tvsat );
  }
}

module_init( tvsat_module_init );
module_exit( tvsat_module_exit );

MODULE_AUTHOR( "Michael Beckers" );
