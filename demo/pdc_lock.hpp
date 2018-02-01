#pragma once
#ifndef _PDC_LOCK_HPP_
#define _PDC_LOCK_HPP_

#include <iostream>
#include <string>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/select.h>
#include <pthread.h>
#include <assert.h>
using namespace std;
struct PdcLock{
    pthread_mutex_t _mutex;
    bool locked;
    string name;

public:
    PdcLock(string nm);
    PdcLock();
    ~PdcLock() {}

    void lock();
    void unlock();
    bool islocked() {return locked;}
    
};


struct PdcCond{
    pthread_cond_t _cond;
    PdcLock *wait_mutex;
	
public:
    PdcCond():wait_mutex(NULL) {
        int r = pthread_cond_init(&_cond,NULL);
        assert(r == 0);
    }
       
    virtual ~PdcCond() { pthread_cond_destroy(&_cond); };

    int wait(PdcLock &mutex){
        //assert(wait_mutex != NULL || wait_mutex == &mutex);
        wait_mutex = &mutex;
        //assert(mutex.islocked());
        //mutex.unlock();
        int r = pthread_cond_wait(&_cond, &mutex._mutex);
        //mutex.lock();
        return r;
    }
    int Signal(){
        int r = pthread_cond_broadcast(&_cond);
        return r;
    }
};

#endif 



