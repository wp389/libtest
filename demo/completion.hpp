#pragma once
#ifndef _COMPLETION_HPP_
#define _COMPLETION_HPP_

#include "type.h"

#include "pdc_lock.hpp"

using namespace std;
//using namespace wp::Pipe;
//using namespace wp::shmQueue;
//using namespace wp::shmMem;
typedef void * pdc_rbd_completion_t;
typedef void (*pdc_callback_t)(pdc_rbd_completion_t c, void *arg);

//(librbd::RBD::AioCompletion *)

struct PdcCompletion{
    void * comp;  //(librbd::RBD::AioCompletion *)
    PdcLock lock;
    PdcCond cond;
    void *read_buf;
    void *write_buf;
    u64  buflen;
    int   retcode;
    int ref;
    u64 opidx;
    Msginfo *op;
    pdc_callback_t callback;
    void * callback_arg;
    bool done;
    struct timeval starttime;
public:
    PdcCompletion(pdc_callback_t cb, void *cb_arg):
         lock("PdcCompletion"),ref(1),done(false)
    {
        ::gettimeofday(&starttime ,NULL);
        callback = cb;
        callback_arg = cb_arg;
    }
    PdcCompletion():lock("PdcCompletion"),ref(1),done(false)
    {}
    ~PdcCompletion() {}
    void setcb(pdc_callback_t cb, void *cb_arg){
        callback = cb;
        callback_arg = cb_arg;
    }
    int get_return_value(){
        return retcode;
    }
    int complete(int r)
    {
        ///static unsigned long long  sum = 0;
        retcode = r;    
        //cerr<<"client get rbd return value :"<< retcode <<endl;
        if(callback)
            callback(comp,callback_arg);
        done = true;
        cond.Signal();

        return 0;
    }
    int wait_for_complete()
    {
        
        lock.lock();
        ref++;
        while (!done)
           cond.wait(lock);
        lock.unlock();
        //ref--;
        //release();
        return 0;
    }
    void release(){
        //lock;
        //NEED todo?
        //unlock;
        ref--;
        //if(ref == 0)
        //    delete this;
    }
};



struct PdcAioCompletion{
    PdcCompletion *pc; //PdcCompletion*
    
public:
    PdcAioCompletion(pdc_callback_t cb, void *cb_arg){
        pc = new PdcCompletion(cb, cb_arg);
        pc->comp = this;
    }
    void setcb(pdc_callback_t cb, void *cb_arg){
        PdcCompletion * c = (PdcCompletion*)pc;
        c->setcb(cb, cb_arg);
    }
    int complete(int r){
        PdcCompletion * c = (PdcCompletion*)pc;
        return c->complete(r);
    }
    int wait_for_complete(){
        PdcCompletion * c = (PdcCompletion*)pc;
        return c->wait_for_complete();
    }
    int aio_get_return_value(){
        PdcCompletion * c = (PdcCompletion*)pc;
        return c->get_return_value();
    }

    void release(){
        PdcCompletion * c = (PdcCompletion*)pc;
        c->release();
        delete this;
		
    }
};

#endif
