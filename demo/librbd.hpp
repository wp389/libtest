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

#ifndef _LIBRBD_HPP_
#define _LIBRBD_HPP_

#include "librbd.h"
typedef void *image_ctx_t;
typedef void *completion_t;
typedef void (*callback_t)(completion_t cb, void *arg);

typedef struct {
  std::string client;
  std::string cookie;
  std::string address;
} locker_t;

typedef rbd_image_info_t image_info_t;



#endif
