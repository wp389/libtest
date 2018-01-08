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

class CephBackend{
    string name;
    string _confpath;

public:
    class RadosClient{
        string radosname;
        string confpath;
    public:
        rados_t cluster;
        rados_ioctx_t ioctx;

    public:
        RadosClient(string nm, string _conf);
        ~RadosClient() {};
        int init();
    
    };
    class RbdVolume{
        string rbdname;
        RadosClient *rados;
        rbd_image_t image;
        //Pipe<Msginfo> pipe;
        
    public:
        RbdVolume(string nm, RadosClient* &_rados): 
            rbdname(nm),rados(_rados)
            //pipe(rbdname.c_str(), 12, PIPEWRITE, PIPECLIENT) 
            {};
        ~RbdVolume() {};
        int init();
        string Getname() {return rbdname;};
    
    };
    map<string , RadosClient*> radoses;
    map<string , RbdVolume *>  vols;
    


public:
    CephBackend(string nm, string confpath);
    ~CephBackend();
    int register_client(map<string,string > &vmclient);
    void *findclient(map<string, string> *opclient);
};



#endif
