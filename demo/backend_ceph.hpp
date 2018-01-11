#pragma once
#ifndef __BACKEND_CEPH_HPP_
#define __BACKEND_CEPH_HPP_

#include "type.h"

#ifdef LOCALTEST
#include "rbd/librbd.h"
#include "rados/librados.h"
#else
#include <rbd/librbd.h>
#include <rados/librados.h>
#endif



//#include "shmmem.hpp"
//#include "pipe.hpp"

using namespace std;
//using namespace wp::Pipe;
//using namespace wp::shmQueue;
//using namespace wp::shmMem;
typedef void * pdc_rbd_completion_t;
typedef void (*pdc_callback_t)(pdc_rbd_completion_t c, void *arg);

//(librbd::RBD::AioCompletion *)
struct PdcCompletion{
    void * comp;  //(librbd::RBD::AioCompletion *)
    void *read_buf;
    void *write_buf;
    u64  buflen;
    int   retcode;
    u64 opidx;
    pdc_callback_t callback;
    void * callback_arg;
public:
    PdcCompletion(pdc_callback_t cb, void *cb_arg, void *c);
    PdcCompletion(void *c);
    ~PdcCompletion() {}
    void setcb(pdc_callback_t cb, void *cb_arg){
        callback = cb;
        callback_arg = cb_arg;
    }

    int complate();
    
    
};

class CephBackend{
    string name;
    string _confpath;
public:
    void *sendmq;
public:
    class RadosClient{
        string radosname;
        string confpath;
    public:
        rados_t cluster;
        rados_ioctx_t ioctx;
        map<string , void *> volumes;
    public:
        RadosClient(string nm, string _conf);
        ~RadosClient() {}
        const char * GetName() {return radosname.c_str();}
        int init();
    
    };
    class RbdVolume{
        string rbdname;
        RadosClient *rados;
        rbd_image_t image;

        //Pipe<Msginfo> pipe;
    public:
            map<string ,void*>mq;
    public:
        RbdVolume(string nm, RadosClient* &_rados): 
            rbdname(nm),rados(_rados)
            //pipe(rbdname.c_str(), 12, PIPEWRITE, PIPECLIENT) 
            {}
        ~RbdVolume() {}  //todo close pipe
        int init();
        const char * GetName() {return rbdname.c_str();}
        int aio_write(u64 offset, size_t len,const char *buf, pdc_rbd_completion_t cb);
    };
    map<string , RadosClient*> radoses;
    map<string , RbdVolume *>  vols;
    


public:
    CephBackend(string nm, string confpath);
    ~CephBackend();
    int register_client(map<string,string > &vmclient, Msginfo *msg);
    void *findclient(map<string, string> *opclient);
};



#endif
