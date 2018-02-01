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
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <memory.h>
#include <memory>
//#define LOCALTEST 1
//#include "pipe.hpp"
typedef unsigned long long u64;
typedef unsigned long u32;

extern u64 msgid;
extern u64 opid;

/*  for all client shm keys is a range 
     from 250 to 1024.
     server default is 123
*/
#define MEMKEY 123
#define PIPEKEY "/tmp/FIFO"
#define PIPESEMKEY 249
#define MEMSEM 23
#define MEMQSEM 45

#define PIPEKEYHEAD "Pipe."
#define SLABSIZE 1024*1024*16+256
#define MEMQSIZE 1024*1024*4

#define CLIENTSTARTPIPEKEY 251
#define SERVERSTARTPIPEKEY 25001

#define NAMELENTH 64
#define SERVERCREATE 1
#define CLIENTNOCREATE 0

#define SERVER_IO_BLACKHOLE 1

#define MULTIPIPE 1
#define CHUNKSIZE 4096
#define EPOLLSIZE 1024

using namespace std;
//using namespace wp::Pipe;
//using namespace pdcPipe::PdcPipe;


typedef enum{
    IOWORKER,
    MSGWORKER,
    EVENTWORKER,

}ThreadType;

struct simpledata{
    char data[CHUNKSIZE];
};
struct pdcdata{
    void * c;
    u64 offset;
    int len;
    int chunksize;
    u64 indexlist[4];

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
        /*
        map<int, pthread_t>::iterator it;
        for(it = threads.begin();it!= threads.end();it++){
            pthread_join(it->second, NULL);
            cerr<< "thread:"<<name<<" ["<<it->first<<"] stoped"<<endl;
	 }
        */
    }
    
    void join(){
        map<int, pthread_t>::iterator it;
        for(it = threads.begin();it!= threads.end();it++){
            pthread_join(it->second, NULL);
            cerr<< "thread:"<<name<<" ["<<it->first<<"] stoped"<<endl;
	 }
    }
    bool stop() {return _stop;}
    bool shutdown() {return false;}
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
        cerr<<"start thread:<<" <<name<<endl;
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
    OPEN_RADOS = 0xa,	//10
    OPEN_RBD,			//11
    PDC_AIO_STAT,		//12
    PDC_ADD_EPOLL,		//13
    PDC_AIO_PREWRITE,	//14
    PDC_AIO_WRITE,		//15
    PDC_AIO_READ,		//16
    GET_MEMORY,			//17
    ACK_MEMORY,			//18
    RW_OP,
    MGR_OP,
    RW_FINISH,
    RW_W_FINISH,		//22
    RW_R_FINISH,			//23
    PUT_SHM,

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

struct pipekey{
    char key[NAMELENTH];  //for client is recv , for server is send
    int semkey;
    char recvkey[NAMELENTH];
    int recvsem;
    pipekey() {};
};
struct  Msginfo{
    bool sw;
    u64 opid;
    pid_t pid;
    pid_t remote_pid;
    int ref;
    pipekey mqkeys;
    
    PdcIomachine opcode;  //opcode
    
    PdcClientInfo client;
    pdcdata data;
    const void * originbuf;
    
    void * op;			//
    void * volume;		//rbd volume info in client or server
    int return_code;
    void *slab;
	
    Msginfo():sw(false),opid(0),remote_pid(0),return_code(0),ref(0) {pid = getpid(); };
	/*init some member,just like the default construction function*/
	void default_init() {
		sw = false;
		opid = 0;
		remote_pid = 0;
		return_code = 0;
		ref = 0;
		pid = getpid();
	}
	/*init some member after reading pipe*/
	void init_after_read() {
		sw = false;
		ref = 0;
		remote_pid = pid;
		pid = getpid();
	}
    void getopid() {opid = ++msgid;}
    void ref_inc() {ref++;}
    void ref_dec() {ref--;}
    bool isdone() {return ref == 0;}
    int copy(Msginfo *m){
        if(m){
            this->opcode = m->opcode;
            this->client = m->client;
            this->remote_pid = m->pid;
            this->data = m->data;
            this->originbuf = m->originbuf;
            this->op = m->op;
            this->opid = m->opid;
            //copy pipe keys:
            ::memcpy(this->mqkeys.key, m->mqkeys.key, sizeof(m->mqkeys.key));
            this->mqkeys.semkey = m->mqkeys.semkey;
            //this->volume = m->volume;   
            this->return_code = m->return_code;
        }
        return 0;
    }
    Msginfo & operator =(const Msginfo*&m){
        if(this != m){
            
            //this->mqkeys.swap(m->mqkeys);
            this->opcode = m->opcode;
            this->client = m->client;
            this->data = m->data;
            this->op = m->op;
            this->remote_pid = m->pid;
            //memset(m , 0 ,);
        }
        return *this;
    }
    void dump(const char *f =NULL){
        struct timeval time;
        if(!sw) return;
        ::gettimeofday(&time ,NULL);
       
        cerr<<"msginfo: at:"<<time.tv_sec<<" s + "<<time.tv_usec<<" us ";
        if(f)
        cerr<<f;
        cerr<<endl;
        cerr<<" ,opref ="<<ref;
        cerr<<" ,opid = "<<opid;
        cerr<<" ,pid ="<<pid;
        cerr<<" ,op = "<<opcode;
        cerr<<" ,client.pool ="<<client.pool;
        cerr<<" ,client.rbd ="<<client.volume;
        //cerr<<" ,client.pipekey ="<<client.pipekey;
        cerr<<" ,offset ="<<data.offset;
        cerr<<" ,len ="<<data.len;

        if(1){
            cerr<<" ,pipe recv keys:"<<mqkeys.key;
            cerr<<" ,sem recv keys:"<<mqkeys.semkey;
            cerr<<" ,pipe send keys:"<<mqkeys.recvkey;
            cerr<<" ,sem send keys:"<<mqkeys.recvsem;

        }

        if(data.chunksize > 0){
            //vector<u64>::iterator it;
            cerr<<" ,indexlist.size:"<<data.chunksize<<" :";
            for(int i = 0;i < data.chunksize;i++){
                cerr<<" "<<i;
            }

        }
        cerr<<endl;
    }
    void insert_op(void *p_op){
        op = p_op;
    }
    void * pop_op(){
        return (void *)op;
    }
    void insert_volume(void *p_v){
        volume = p_v;
    }
    void * pop_volume(){
        return (void *)volume;
    }
    int getreturnvalue() {return return_code;}
    
};

class Center{
	
public:
    Center() {};
    ~Center() {};
};


typedef int ( * EnqFn)(Msginfo *op);





