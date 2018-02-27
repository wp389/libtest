#pragma once
#ifndef __BACKEND_CEPH_HPP_
#define __BACKEND_CEPH_HPP_

#include "type.h"


#include <rbd/librbd.h>
#include <rados/librados.h>

#include "completion.hpp"
#include "pdc_lock.hpp"

using namespace std;

class CephBackend{
    string name;
    string _confpath;
public:
    void *sendmq;
    list<Msginfo *> *_queue;
public:
    class RadosClient{
        string radosname;
        string confpath;
    public:
        CephBackend *ceph;
    public:
        rados_t cluster;
        rados_ioctx_t ioctx;
        map<string , void *> volumes;
    public:
        RadosClient(string nm, string _conf, CephBackend *_ceph);
        ~RadosClient() {}
        const char * GetName() {return radosname.c_str();}
        int init(int create);
    
    };
    class RbdVolume{
        string rbdname;
        RadosClient *rados;
        rbd_image_t image;
    public:
            map<string ,void*>mq;
    public:
        RbdVolume(string nm, RadosClient* _rados): 
            rbdname(nm),rados(_rados)
            {}
        ~RbdVolume() {}  //todo close pipe
        int init(int create);
        const char * GetName() {return rbdname.c_str();}
        int do_create_rbd_completion(void * op, rbd_completion_t *comp );
        int do_aio_write(void *_op,u64 offset, size_t len,const char *buf, pdc_rbd_completion_t c);
        int do_aio_read(void *_op,u64 offset, size_t len, char *buf, pdc_rbd_completion_t c);
        int do_aio_flush(void *_op, rbd_completion_t c);
    };
    map<string , RadosClient*> radoses;
    map<string , RbdVolume *>  vols;
    


public:
    CephBackend(string nm, string confpath, list<Msginfo *>* msgop);
    ~CephBackend();
    int register_client(map<string,string > &vmclient, Msginfo *msg);
    void *findclient(map<string, string> *opclient);
    void *findclientnew(string &rados, string &rbd);
};


extern void pdc_callback(rbd_completion_t cb, void *arg);
#endif
