#pragma once
#ifndef __PDCSERVER_HPP_
#define __PDCSERVER_HPP_

//#include "type.h"
//#include "shmmem.hpp"
//#include "pipe.hpp"
#include "backend_ceph.hpp"
#include "shmmem.hpp"
#include "pipe.hpp"
#include "type.h"


using namespace std;
//using namespace wp::Pipe;
//using namespace wp::shmQueue;
//using namespace wp::shmMem;
using namespace shmMem;
using namespace pdcPipe;

class Pdcserver :public Center{
    string servername;
    Perfs perf;
    Time time;
    pid_t pid;
    int threadnum;
    map<int  , ThreadType >theads;
    PdcPipe<Msginfo> msgmq;
    
    map<map<string, string>, PdcPipe<Msginfo>* > ackmq;
    //list<PdcOp> queue_io;
    list<Msginfo> queue_ms;
    Perfs *performace;
    ShmMem<simpledata> slab;
    map<string,CephBackend *> clusters;
	
public:
    class Iothreads :public Threadpool{
        string desc;
        Pdcserver *server;
    public:
        Iothreads(string desc_, Pdcserver *_server):desc(desc_),server(_server),Threadpool() {}
        ~Iothreads() {};
        
        int do_op(void * m);
        void *_process();
    };
	
    class Finisherthreads :public Threadpool{
        string desc;
        Pdcserver *server;
    public:
        Finisherthreads(string desc_,Pdcserver * _server):
            desc(desc_),server(_server),Threadpool() {};
        ~Finisherthreads() {};
        virtual void* _process();
    };	
    class Msgthreads :public Threadpool{
        string desc;
        Pdcserver *server;
    public:
        Msgthreads(string desc_, Pdcserver *_server):
            desc(desc_),server(_server),Threadpool() {};
       ~Msgthreads() {};
        virtual void *_process();
    };

public:
    pthread_mutex_t iomutex;
    list<Msginfo *> ops;
    Iothreads  *iothread;

    pthread_mutex_t finimutex;
    list<Msginfo *> finishop;
    Finisherthreads *finisher;

    pthread_mutex_t msgmutex;
    list<Msginfo *>msgop;
    Msgthreads *msgthread;
    
    
public:
    Pdcserver(string nm): perf(),time(),servername(nm),pid(-1),threadnum(2),
        msgmq(PIPEKEY, PIPESEMKEY, PIPEREAD, PIPESERVER),
        performace(NULL),
        iothread(NULL), msgthread(NULL), finisher(NULL),
        slab(MEMKEY, SERVERCREATE)
        {pid = getpid();
            time.reset();
        }
    ~Pdcserver();
    void do_work(Pdcserver *server);
    int init();
    void OpFindClient(Msginfo *&op);
    int register_vm(map<string,string > &client, Msginfo *msg);
    int unregister();
    int register_connection(Msginfo* msg);

	
};



#endif
