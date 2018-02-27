#pragma once
#ifndef __PDCCLIENT_HPP
#define __PDCCLIENT_HPP

//#include "type.h"
//#include "shmmem.hpp"
//#include "pipe.hpp"
#include "backend_client.hpp"
#include "shmmem.hpp"
#include "pipe.hpp"
#include "type.h"
#include "mempool.hpp"


using namespace std;
//using namespace wp::Pipe;
//using namespace wp::shmQueue;
//using namespace wp::shmMem;
using namespace shmMem;
using namespace pdcPipe;

typedef void*  pdc_rados_ioctx_t;
typedef void*  pdc_rados_t;
typedef void*  pdc_rbd_image_t;




class PdcClient :public Center{
    string clientname;
    Perfs perf;
    Time time;
    pid_t pid;
    int threadnum;
    int autokey;
    int ref;
public:
    map<int  , ThreadType >theads;
    //PdcPipe<PdcOp> pdc
    
    PdcPipe<Msginfo> msgmq;
    PdcPipe<Msginfo>::ptr sendmq;
    PdcPipe<Msginfo>::ptr ackmq;
    map<string , void*> mq;
    //map<map<string, string>, PdcPipe<Msginfo>* > ackmq;
    //list<PdcOp> queue_io;
    Perfs *performace;
    ShmMem slab;
    map<string,BackendClient *> clusters;
	MemPool<Msginfo> msg_pool;
    friend class BackendClient;
public:
    class Iothreads :public Threadpool{
        string desc;
        PdcClient *pc; //pdcclient
    public:
        Iothreads(string desc_, PdcClient *_pc):
			desc(desc_),pc(_pc) {}
        ~Iothreads() {}
        
        int do_op(void * m);
        virtual void *_process();
    };
	
    class Finisherthreads :public Threadpool{
        string desc;
        PdcClient *pc;
    public:
        Finisherthreads(string desc_, PdcClient * _pc):
			desc(desc_),pc(_pc) {}
        ~Finisherthreads() {}
        virtual void* _process();
    };	
    class Msgthreads :public Threadpool{
        string desc;
        PdcClient *pc;
    public:
        Msgthreads(string desc_, PdcClient *_pc):
			desc(desc_), pc(_pc) {}
        ~Msgthreads() {}
        virtual void *_process();
    };

public:
    PdcCond opcond;
    PdcLock iolock;
    list<Msginfo *> ops;
    Iothreads  *iothread;

    PdcCond listencond;
    PdcLock listenlock;
    list<Msginfo *> listenop;
    Finisherthreads *listen;

    int msg_working;
    PdcCond msgcond;
    PdcLock msglock;
    list<Msginfo *>msgop;
    Msgthreads *msgthread;
    
    
public:
    PdcClient(string nm): 
        perf(),time(),clientname(nm),pid(-1),threadnum(2),
        //autokey(250),  //pipe key start from 250
        msgmq(PIPEKEY, MEMQSEM, PIPEWRITE, PIPECLIENT),
        sendmq(NULL),
        ackmq(NULL),
        performace(NULL),
        iothread(NULL), msgthread(NULL), listen(NULL),
        slab(MEMKEY, CLIENTNOCREATE),
        ref(0),iolock("client-iolock"),
        listenlock("client-listenlock"),
        msglock("client-msglock")
        {
            pid = getpid();
            time.reset();
            //PdcClient do not need reset mq keys:
            ref++;
            //string key;
            //key = PIPEKEYHEAD + 
            //msgmq.ResetPipeKey();
        }
    ~PdcClient() {};
    /*
    PdcClient * operator=(const PdcClient *& pclient){
        PdcClient *tmp = this;
        this = pclient;
        pclient = tmp;
        return this;
    }
    */
    bool has_ackmq() {return ackmq != NULL;}
    void do_work(PdcClient *pc);
    int init();
    void inc_ref() {ref ++;}
    void OpFindClient(Msginfo *&op);
    int enqueue(Msginfo *op){
        msglock.lock();
        msgop.push_back(op);
        msgcond.Signal();
        msglock.unlock();
        
    }
    int aio_write(BackendClient::RbdVolume *prbd, u64 offset, size_t len,const char *buf, PdcCompletion *c);
    int aio_read(BackendClient::RbdVolume *prbd, u64 offset, size_t len,const char *buf, PdcCompletion *c);
    int aio_flush(BackendClient::RbdVolume *prbd,  PdcCompletion *c);
};




extern PdcClient *pdc_client_mgr;


#endif
