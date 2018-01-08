/*global head file
*  this is for dftdc 
*
*/
#pragma once
#include <stdio.h>
#include <string>
#include <map>
#include <iostream>
#include <vector>
#include <list>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#define LOCALTEST 1
//#include "pipe.hpp"
typedef unsigned long long u64;
typedef unsigned long u32;

#define MEMKEY 123
#define PIPEKEY "/tmp/FIFO"
#define MEMSEM 12
#define MEMQSEM 45

#define SLABSIZE 1024*1024*16+256
#define MEMQSIZE 1024*1024*4


#define NAMELENTH 64
#define SERVERCREATE 1
using namespace std;
//using namespace wp::Pipe;
//using namespace pdcPipe::PdcPipe;



typedef enum{
    IOWORKER,
    MSGWORKER,
    EVENTWORKER,

}ThreadType;

struct simpledata{
    char data[4096];
};
struct pdcdata{
    int len;
    list<u64> indexlist;

};

class Threadpool{
    pthread_t id;
    ThreadType type;
    string name;
    bool _stop;
    int index;
    int size;
    map<int ,pthread_t>threads;
    
public:
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    
public:
    
    //Threadpool(const Threadpool& other);
    //const Threadpool& Threadpool=(const Threadpool& other);
    Threadpool(): size(1),_stop(true) {};
     ~Threadpool(){
        _stop = false;
        map<int, pthread_t>::iterator it;
        for(it = threads.begin();it!= threads.end();it++){
            pthread_join(it->second, NULL);
            cerr<< "thread:"<<name<<" ["<<it->first<<"] stoped"<<endl;
	 }

    };
    bool stop() {return _stop;}
protected:
    virtual void *_process() = 0;
public:
    void* _entry(){
        return _process();
    }

private:
    static void *_entry_func(void *arg){
        void *r = ((Threadpool*)arg)->_entry();
            return r;
    }
public:
    int init(int threadnum){
        size = threadnum;
        //mutex = PTHREAD_MUTEX_INITIALIZER;
        //cond = PTHREAD_COND_INITALIZER;
        pthread_mutex_init(&mutex,NULL);
        pthread_cond_init(&cond, NULL);
        for(int i=0;i < size; i++){
            pthread_t id;
            pthread_create(&id, NULL , _entry_func, this);
            threads.insert(pair<int, pthread_t>(i, id) );

        }
        return 0;
    }
    void start(){
        _stop = false;
    }
    	
};


class Time{
    struct timeval s;
    time_t curtime;
    
public:
    Time() {::gettimeofday(&s,NULL);}
    double timediff()
    {
	    double diff;
	    time_t ss;
	    suseconds_t u;
	    struct timeval now;

	    ::gettimeofday(&now ,NULL);
	    ss = now.tv_sec - s.tv_sec;
	    u =  now.tv_usec - s.tv_usec;

	    diff = ss;
	    diff *= 1000000.0;
	    diff += u;

	    return diff;
    }
    void reset() {::gettimeofday(&s,NULL);}
    time_t dump(){
        time(&curtime);
        return curtime;
    }
};

typedef enum {
    OPEN_RBD = 0xa,
    GET_MEMORY,
    ACK_MEMORY,
    RW_OP,
    MGR_OP,
    

}PdcIomachine;


struct Times{
	struct timeval time;
	u64 ref;
	Times();
};

struct Perfs{
    struct timeval t;
    map<string , u64> io;
    

public:
    
    map<PdcIomachine, Times> perf;
public:
    Perfs() {};
    ~Perfs() {};
    void reset(){
        perf.clear();
    }

    void resettime(){
        gettimeofday(& t, NULL);
		
    }
    void inc(PdcIomachine &state){
        //Times *time;
        struct timeval end;
        gettimeofday(&t, NULL);
        /*
        if(perf.count(state) == 0){
            perf[state] = 1;
            
            
        }else{
            perf[state]++;

        }
        */
        //perf
    }

};
struct PdcClientInfo{
    char cluster[NAMELENTH];
    char pool[NAMELENTH];
    char volume[NAMELENTH];
    int   pipekey;
    
    u32 offset;
    u32 len;
    
};

struct  Msginfo{
    pid_t pid;
    PdcIomachine op;
    PdcClientInfo client;
    pdcdata data;
    
    	
};

class Center{
	
public:
    Center() {};
    ~Center() {};
};



struct PdcOp{
    u64 op_idx;
    pid_t pid;
    pdcdata data;
    PdcClientInfo client;
    void *volume;
    int ret;
public:
    PdcOp() {};
    
};

