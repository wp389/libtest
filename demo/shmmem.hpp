#pragma once
#ifndef _SHMMEM_HPP_
#define _SHMMEM_HPP_
//#pragma once
#include <iostream>
#include <string>
#include <sys/sem.h>
#include <sys/shm.h>
#include <string.h>
#include <stdint.h>

#include "pdc_lock.hpp"
#define SHMQUEUEOK 1
#define SHMQUEUEERROR -1
#define SHMKEY   2333
#define SEMKEY   2333

//namespace wp {
namespace shmMem {


class SemLock {
public:
    union semun {
        int val; /* value for SETVAL */

        struct semid_ds *buf; /* buffer for IPC_STAT, IPC_SET */

        unsigned short *array; /* array for GETALL, SETALL */

        struct seminfo *__buf; /* buffer for IPC_INFO */
    };

    SemLock() :m_iSemId(-1), m_isCreate(-1) {

    }
    ~SemLock() {

    }
    int Init(int key) {
        m_iSemId = ::semget(key, 1, 0);
        if (m_iSemId < 0) {
            m_iSemId = ::semget(key, 1, IPC_CREAT| 0666);
            m_isCreate = 1;
        }
        if (m_iSemId < 0) {
            m_sErrMsg.clear();
            m_sErrMsg = "semget error ";
            return m_iSemId;
        }
        if (m_isCreate == 1) {
            union semun arg;
            arg.val = 1;
            int ret = ::semctl(m_iSemId, 0, SETVAL, arg.val);
            if (ret < 0) {
                m_sErrMsg.clear();
                m_sErrMsg = "sem setval error ";
                return ret;
            }
        }
        return m_iSemId;
    }

    int Lock() {
        union semun arg;
        int val = ::semctl(m_iSemId, 0, GETVAL, arg);
        if (val == 1) {
            struct sembuf sops = { 0,-1, SEM_UNDO };
            int ret = ::semop(m_iSemId, &sops, 1);
            if (ret < 0) {
                m_sErrMsg.clear();
                m_sErrMsg = "semop -- error ";
                return ret;
            }
        }
        return 0;
    }
    int unLock() {
        union semun arg;
        int val = ::semctl(m_iSemId, 0, GETVAL, arg);
        if (val == 0) {
            struct sembuf sops = { 0,+1, SEM_UNDO };
            int ret = ::semop(m_iSemId, &sops, 1);
            if (ret < 0) {
                m_sErrMsg.clear();
                m_sErrMsg = "semop ++ error ";
                return ret;
            }
        }
        return 0;
    }
    std::string GetErrMsg() {
        return m_sErrMsg;
    }

    private:
    int m_iSemId;
    int m_isCreate;
    std::string m_sErrMsg;
};

class semLockGuard {
public:
    semLockGuard(SemLock &sem) :m_Sem(sem) {
        m_Sem.Lock();
    }
    ~semLockGuard() {
        m_Sem.unLock();
    }
private:
    SemLock &m_Sem;
    };


static const uint32_t SHMSIZE = 1024*1024*1024+1024;
//测试将大小调小，实际使用时应该设大避免影响性能

class SuperBlock {
public:
    u64 DataCount;
    u64 Front;
    u64 Rear;
    u64 AllCount;
    u64 AllSize;
    u64 ItemSize;
    u64 Inuse;
    u64 Avalid;
public:
    SuperBlock() :DataCount(0), Front(0), Rear(0),
        AllCount(0), AllSize(0), ItemSize(0){
        
    }
};


template<typename T>
class ShmMem {
public:
    explicit ShmMem(int key,int iCreate=-1):m_pShm(NULL),m_iStatus(-1), m_iShmId(-1), 
        m_key(key),m_iCreate(iCreate),usetype(0),lock("shmlock"){
        m_pShm = NULL;
    }

    ~ShmMem() {
        ::shmdt(m_pShm);
    }
    int Init(int _usetype=1){
        int ret = m_Sem.Init(m_key);
        if (ret < 0) {
            m_sErrMsg.clear();
            m_sErrMsg = m_Sem.GetErrMsg();
            return ret;
        }
        m_iShmId = ::shmget(m_key,SHMSIZE,0);
        if (m_iShmId < 0) {
            m_iCreate = 1;
            m_iShmId = ::shmget(m_key, SHMSIZE, IPC_CREAT);
            
        }
        if (m_iShmId < 0) {
            m_sErrMsg.clear();
            m_sErrMsg = "shmget error ";
            m_iStatus = SHMQUEUEERROR;
            return m_iShmId;
        }

        m_pShm = (char*)::shmat(m_iShmId,NULL,0);//读写模式；
        if (m_pShm == NULL) {
            m_sErrMsg.clear();
            m_sErrMsg = "shmat error ";
            return -1;
        }
        //attach superblock
        sb = (SuperBlock*)m_pShm;
        
        if (m_iCreate == 1 ) {
            if(usetype == 0){
                cerr<<"init to memory queue model"<<endl;
                //QueueHead Head;
                sb->ItemSize= sizeof(T);
                sb->AllSize = SHMSIZE  - 1024;
                sb->AllCount = sb->AllSize / sb->ItemSize;
                sb->DataCount = 0;
                sb->Front = 0;
                sb->Rear = 0;
                sb->Avalid = sb->AllCount;
                sb->Inuse = 0;
                freelist.resize(sb->AllCount);
                usedlist.resize(sb->AllCount);
                //freelist()
                //::memcpy(m_pShm,&Head,sizeof(QueueHead));
            }else if(usetype ==1){
            
            }
        }
        
        return m_iShmId;
    }

    bool isEmpty() {  //whether had memory to used?
	 if(sb->Avalid > 0)
            return false;
	 else if(sb->Avalid == 0)
	     return true;
	 else{
            cerr<<"superblock is:"<<sb<<endl;
            assert(0);
            return true;
	  }
    
    }

    bool isFull() { // empty  == full ?
        if(sb->Avalid > 0)
            return false;
        else if(sb->Avalid == 0)
            return true;
        else{
            cerr<<"superblock is check full error:"<<sb<<endl;
            assert(0);
            return true;
        }
    }

    int getSize() {

        return sb->DataCount;
    }
    u64 getAvalid(){
        return sb->Avalid;
    }
    /* if in one simple thread ,do not use lock
    *   but ,when use in multithreads ,a lock is needed.
    */
    int  get(u32 size, u64* sum){  //Get memory block, 
        //semLockGuard oLock(m_Sem);
        lock.lock();
        //list<u64> sum;
        int n =0;
        if (isFull() ) {
            cerr<<"shm had no memory, can not get one"<<endl;
            assert(0);
            return  -1;
        }
        if((getAvalid() *(sb->ItemSize)) < size ){
            cerr<<"shm had no memory, can not get one"<<endl;
            assert(0);
            return -1;
        }
        u32 count = 0;
        count = size /sb->ItemSize;
        count = ((count * sb->ItemSize ) >=  size)? count :count +1;
        
        while(count > 0){
            //u64 tmp 
            if(sb->Front < sb->AllCount){
                for(u64 i = sb->Front; i< sb->AllCount && count > 0;i++){
                    count--;
                    sum[n++] = i;
                    //usedlist.push_back(i);	//for now ,we do not need usedlist
                    //sum.push_back(i);
                    sb->Front = sb->Front+1;
                    sb->Inuse++;
                    sb->Avalid--;
                }
                
            }else{
                
                assert(freelist.capacity() >= count);
                if(sb->Avalid == 0){
                    lock.unlock();
		       return -1;
                }
                count--;
                
                sum[n++] = freelist.back();
                //sum.push_back(freelist.front());
                //freelist.pop_front();   //for list
                freelist.pop_back();		//for vertor
                //freelist.erase(freelist.begin());  //for vetor
                sb->Inuse++;
                sb->Avalid--;

            }
            
    	}
       assert(sb->Inuse + sb->Avalid == sb->AllCount);
	lock.unlock();
       //assert((sb->Inuse + sb->Avalid) == sb->AllCount);
       return n;
        
    }

    T * getaddbyindex(u64 index){
        if(index  > sb->AllCount ){
            cerr<<"shm count max is:"<<sb->AllCount<<" now use:"<<index<<endl;;
        }

        return (T *)(m_pShm + 1024 + index * sizeof(T));
    }
    int put(vector<u64> &used){
        //semLockGuard oLock(m_Sem);
        vector<u64>::iterator it =used.begin();
        int size = 0;
        if(it == used.end())  return -2;

        lock.lock();
        for(;it<used.end();it++){
            freelist.push_back(*it);
            size++;
        }
        //freelist.assign(used.begin(), used.end());
        //freelist.splice(freelist.end(), used, used.begin(), used.end()); //for list
        sb->Avalid  += size;
        sb->Inuse -= size;
        assert(sb->Inuse + sb->Avalid == sb->AllCount);
        lock.unlock();
        
        return size;
    }


    std::string GetErrMsg() {
        return m_sErrMsg;
    }
    private:
    SuperBlock*sb;
    int usetype;
    char *m_pShm;
    int m_iShmId;
    int m_iStatus;
    int m_iCreate;
    int m_key;
    vector<u64> freelist;
    vector<u64> usedlist;
    char *pdata;
    T *pmem;
    SemLock m_Sem;
    PdcLock lock;
    std::string m_sErrMsg;
};


}
//}

#endif 



