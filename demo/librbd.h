// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2011 New Dream Network
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.	See file COPYING.
 *
 */

#ifndef PDC_LIBRBD_H
#define PDC_LIBRBD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
//#include "type.h"

typedef unsigned long long u64;

#define LIBRBD_VER_MAJOR 0
#define LIBRBD_VER_MINOR 1
#define LIBRBD_VER_EXTRA 9

#define LIBRBD_VERSION(maj, min, extra) ((maj << 16) + (min << 8) + extra)

#define LIBRBD_VERSION_CODE LIBRBD_VERSION(LIBRBD_VER_MAJOR, LIBRBD_VER_MINOR, LIBRBD_VER_EXTRA)

#define LIBRBD_SUPPORTS_WATCH 0
#define LIBRBD_SUPPORTS_AIO_FLUSH 1
#define LIBRBD_SUPPORTS_INVALIDATE 1

/*
#if __GNUC__ >= 4
  #define CEPH_RBD_API    __attribute__ ((visibility ("default")))
#else
  #define CEPH_RBD_API
#endif
*/
#define RBD_FLAG_OBJECT_MAP_INVALID   (1<<0)

typedef void* rbd_snap_t;
typedef void* rbd_image_t;
typedef void* rados_ioctx_t;
typedef void* rados_t;


typedef struct {
  u64 id;
  u64 size;
  const char *name;
} rbd_snap_info_t;

#define RBD_MAX_IMAGE_NAME_SIZE 96
#define RBD_MAX_BLOCK_NAME_SIZE 24

typedef struct {
  u64 size;
  u64 obj_size;
  u64 num_objs;
  int order;
  char block_name_prefix[RBD_MAX_BLOCK_NAME_SIZE];
  u64 parent_pool;			      /* deprecated */
  char parent_name[RBD_MAX_IMAGE_NAME_SIZE];  /* deprecated */
} rbd_image_info_t;

void rbd_version(int *major, int *minor, int *extra);


int rbd_open(rados_ioctx_t io, const char *name,
                          rbd_image_t *image, const char *snap_name);


int rbd_close(rbd_image_t image);


int rbd_stat(rbd_image_t image, rbd_image_info_t *info,
                          size_t infosize);


typedef void *rbd_completion_t;
typedef void (*rbd_callback_t)(rbd_completion_t cb, void *arg);

int rbd_create(rados_ioctx_t io, const char *name, uint64_t size,
                            int *order);
int rbd_resize(rbd_image_t image, uint64_t size);

int rbd_snap_create(rbd_image_t image, const char *snapname);
int rbd_snap_remove(rbd_image_t image, const char *snapname);
int rbd_snap_rollback(rbd_image_t image, const char *snapname);

int rbd_aio_discard(rbd_image_t image, uint64_t off, uint64_t len,
			       rbd_completion_t c);

int rbd_snap_list(rbd_image_t image, rbd_snap_info_t *snaps,
                               int *max_snaps);
void rbd_snap_list_end(rbd_snap_info_t *snaps);

int rbd_snap_rollback(rbd_image_t image, const char *snapname);

int rbd_aio_write(rbd_image_t image, u64 off, size_t len,
                               const char *buf, rbd_completion_t c);

int rbd_aio_read(rbd_image_t image, u64 off, size_t len,
                              char *buf, rbd_completion_t c);

int rbd_aio_flush(rbd_image_t image, rbd_completion_t c);

int rbd_aio_create_completion(void *cb_arg,
                                           rbd_callback_t complete_cb,
                                           rbd_completion_t *c);

int rbd_aio_is_complete(rbd_completion_t c);


int rbd_aio_wait_for_complete(rbd_completion_t c);
ssize_t rbd_aio_get_return_value(rbd_completion_t c);
void rbd_aio_release(rbd_completion_t c);



#ifdef __cplusplus
}
#endif

#endif
