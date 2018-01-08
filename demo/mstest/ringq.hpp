#ifndef _SHMQ_HPP_
#define _SHMQ_HPP_
//#pragma once
#include <iostream>
#include <string>
#include <sys/sem.h>
#include <sys/shm.h>
#include <string.h>
#include <stdint.h>
#define SHMQUEUEOK 1
#define SHMQUEUEERROR -1
#define SHMKEY   2333
#define SEMKEY   2333

//namespace wp {
namespace shmQueue {


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
    semLockGuard(SemLock sem) :m_Sem(sem) {
        m_Sem.Lock();
    }
    ~semLockGuard() {
        m_Sem.unLock();
    }
private:
    SemLock &m_Sem;
    };


static const uint32_t SHMSIZE = 1024*1024*16+24;
//测试将大小调小，实际使用时应该设大避免影响性能

class QueueHead {
public:
    int32_t uDataCount;
    int32_t uFront;
    int32_t uRear;
    int32_t uAllCount;
    int32_t uAllSize;
    int32_t uItemSize;
public:
    QueueHead() :uDataCount(0), uFront(0), uRear(0),
        uAllCount(0), uAllSize(0), uItemSize(0){
    }
};


template<typename T>
class shmQueue {
public:
    explicit shmQueue(int key,int iCreate=-1):m_pShm(NULL),m_iStatus(-1), m_iShmId(-1), 
        m_key(key),m_iCreate(iCreate){
        m_pShm = NULL;
    }

    ~shmQueue() {
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

        if (m_iCreate == 1 ) {
            if(usetype == 0){
                cerr<<"init to memory queue model"<<endl;
                QueueHead Head;
                Head.uItemSize = sizeof(T);
                Head.uAllSize = SHMSIZE - 20;
                Head.uAllCount = Head.uAllSize / Head.uItemSize;
                Head.uDataCount = 0;
                Head.uFront = 0;
                Head.uRear = 0;
                ::memcpy(m_pShm,&Head,sizeof(QueueHead));
            }else if(usetype ==1){
            
            }
        }

       T *t = new T();
        
        return m_iShmId;
    }

    bool isEmpty() {
        QueueHead Head;
        
        memcpy(&Head,m_pShm,sizeof(QueueHead));

        return Head.uFront == Head.uRear;
    }

    bool isFull() {
        QueueHead Head;
        memcpy(&Head, m_pShm, sizeof(QueueHead));

        return Head.uFront == (Head.uRear + 1) % Head.uAllCount;
    }

    int getSize() {
        QueueHead Head;
        memcpy(&Head, m_pShm, sizeof(QueueHead));

        return Head.uDataCount;
    }

    map<u64,T*> *get(){
        if (isFull()) {
            cerr<<"shm is full, can not get one"<<endl;
            assert(0);
            return NULL;
        }
        QueueHead *head ;
        head = (QueueHead*)m_pShm;
        pdata = (T *)(m_pShm + sizeof(QueueHead) + head->uRear * sizeof(T));
        head->uRear = ((head->uRear + 1) % head->uAllCount);
        
        return pair<u64,pdata>;
        
    }
    int put(){
        if (isEmpty()) {
            cerr<<"shm is empty, can not put"<<endl;
            return -1;//应该选择抛出异常等方式，待改进；
        }
        
    }
    int  push(T*& a){

        //semLockGuard oLock(m_Sem);

        if (isFull()) {
            return -1;
        }
        QueueHead Head;
        memcpy(&Head, m_pShm, sizeof(QueueHead));

        char *p = m_pShm + sizeof(QueueHead) + Head.uRear * sizeof(T);
        // *head->uRear =  (uRear + 1) % uAllCount;

        memcpy(p,a,sizeof(T));
        Head.uRear = ((Head.uRear + 1) % Head.uAllCount);
        memcpy(m_pShm,&Head,sizeof(QueueHead));
        //*((uint32_t*)(m_pShm + sizeof(int) * 2)) = (oQueryHead.uRear + 1) % oQueryHead.uAllCount;

    return 0;
    }

    T *pop() {

        //semLockGuard oLock(m_Sem);

        if (isEmpty()) {
            return NULL;//应该选择抛出异常等方式，待改进；
        }
        QueueHead Head;
        memcpy(&Head, m_pShm, sizeof(QueueHead));

        char* p = m_pShm + sizeof(QueueHead) + Head.uFront * sizeof(T);

        //T *odata = new T();
        memcpy(t,p,sizeof(T));
        Head.uFront = ((Head.uFront + 1) % Head.uAllCount);
        memcpy(m_pShm,&Head,sizeof(QueueHead));
        //*((uint32_t*)(m_pShm+sizeof(int)*1))= ((oQueryHead.uFront+ 1) % oQueryHead.uAllCount);

        return t;
    }

    std::string GetErrMsg() {
        return m_sErrMsg;
    }
    private:
    list<T *> memorylist;
    string usetype;
    T *t;
    char *m_pShm;
    int m_iShmId;
    int m_iStatus;
    int m_iCreate;
    int m_key;
    char *pdata;
    SemLock m_Sem;
    std::string m_sErrMsg;
};


}
//}

#endif 



