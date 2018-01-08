#pragma once
#ifndef __LIBPDC_H__
#define __LIBPDC_H__

//#include "type.h"
//#include "shmmem.hpp"
//#include "pipe.hpp"
#include "backend_ceph.hpp"
#include "shmmem.hpp"
#include "pipe.hpp"
#include "type.h"


typedef void*  pdc_rados_ioctx_t;
typedef void*  pdc_rados_t;
typedef void*  rbd_image_t;


class PdcRados{
    librados::RadosClient *radosclient;
    
    rados_t cluster;
    rados_ioctx_t rados_ioctx;
    pid_t pid;
    
public:
    PdcRados(pdc_rados_t &pdcrados);
    ~PdcRados();

    int pdc_connect();
    int pdc_create_ioctx(pdc_rados_t rados, pdc_rados_ioctx_t &ioctx);
    
    
};

class PdcRbd{



public:


};

extern pdc_rados_t pdc_rados_create(rados_t *cluster, const char * const id);

extern int pdc_rados_connect();

extern int pdc_rados_create_ioctx(pdc_rados_t rados, pdc_rados_ioctx_t &ioctx);

extern int pdc_rbd_open(pdc_rados_ioctx_t ioctx, const char * imagename, rbd_image_t image, int snap_id);



#endif
