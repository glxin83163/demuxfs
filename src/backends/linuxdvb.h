#ifndef __linuxdvb_h
#define __linuxdvb_h

#ifdef USE_LINUXDVB

#define _GNU_SOURCE
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/dvb/dmx.h>

#endif /* USE_LINUXDVB */

#endif /* __linuxdvb_h */