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
    u64 opidx;
    Msginfo *op;
    pdc_callback_t callback;
    void * callback_arg;
    bool done;
    struct timeval starttime;
public:
    PdcCompletion(pdc_callback_t cb, void *cb_arg, void *c):
         comp(c),lock("PdcCompletion"),done(false)
    {
        ::gettimeofday(&starttime ,NULL);
        callback = cb;
        callback_arg = cb_arg;
    }
    PdcCompletion(void *c):comp(c),lock("PdcCompletion"),done(false)
    {}
    ~PdcCompletion() {}
    void setcb(pdc_callback_t cb, void *cb_arg){
        callback = cb;
        callback_arg = cb_arg;
    }

    int complate(int r)
    {
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
        while (!done)
           cond.wait(lock);
        lock.unlock();
        return 0;
    }
    void release(){
        //lock;
        //NEED todo?
        //unlock;
        delete this;
    }
};

#endif
