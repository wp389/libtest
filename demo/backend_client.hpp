#pragma once
#ifndef __BACKEND_CLIENT_HPP_
#define __BACKEND_CLIENT_HPP_

#include "type.h"

#include "completion.hpp"
#include "pdc_lock.hpp"


using namespace std;


class BackendClient{
    string name;
    string _confpath;
public:
    void *sendmq;
    list<Msginfo *> *_queue;
    pthread_mutex_t *_mutex;
public:
    class RadosClient{
        string radosname;
        string confpath;
    public:
        BackendClient *ceph;
    public:
        map<string , void *> volumes;
    public:
        RadosClient(string nm, string _conf,BackendClient *_ceph);
        ~RadosClient() {}
        const char * GetName() {return radosname.c_str();}
        int init(int create);
    
    };
    class RbdVolume{
        string rbdname;
        RadosClient *rados;

    public:
            map<string ,void*>mq;
    public:
        RbdVolume(string nm, RadosClient* _rados): 
            rbdname(nm),rados(_rados)
            //pipe(rbdname.c_str(), 12, PIPEWRITE, PIPECLIENT) 
            {}
        ~RbdVolume() {}  //todo close pipe
        int init(int create);
        const char * GetName() {return rbdname.c_str();}
        int aio_write(u64 offset, size_t len,const char *buf, pdc_rbd_completion_t cb);
        //static void pdc_callback(rbd_completion_t cb, void *arg);
        //int do_create_rbd_completion(void * op, rbd_completion_t *comp );
        //int do_aio_write(void *_op,u64 offset, size_t len,const char *buf, pdc_rbd_completion_t c);

    };
    map<string , RadosClient*> radoses;
    map<string , RbdVolume *>  vols;
    


public:
    BackendClient(string nm, string confpath, list<Msginfo *>* msgop);
    ~BackendClient() {}

    void *findclient(map<string, string> *opclient);
};


#endif
