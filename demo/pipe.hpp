#pragma once
#ifndef _PIPE_HPP_
#define _PIPE_HPP_

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

#define SHMQUEUEOK 1
#define SHMQUEUEERROR -1
#define PIPEWRITE   1
#define PIPEREAD   2

#define PIPEMSGSIZE 8192
#define SENDMQ "send"
#define RECVMQ "recv"
using namespace std;
//namespace wp {
namespace pdcPipe {
	struct pkey{
		char key[NAMELENTH];
		int semkey;
		
	};

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


		static const uint32_t SHMSIZE = 1024*1024*16+24;
//测试将大小调小，实际使用时应该设大避免影响性能
//PIPECLIENT need init keys, and than sync this keys to pdcserver. 
//update them to the same key
// 

             typedef enum{
                 PIPECLIENT,
                 PIPESERVER,
             }SYS_t;
    template<typename T>
    class PdcPipe {
    public:
        explicit PdcPipe(SYS_t systype):
            m_key(""),m_model(0),semkey(-1),s_type(systype),
        fd(-1){
            if(systype == PIPECLIENT)
            m_model = O_WRONLY|O_NONBLOCK;  //|O_NONBLOCK
            if(systype == PIPESERVER)
            m_model = O_RDONLY|O_NONBLOCK;
            //m_key
        }		
        explicit PdcPipe(const char *  key, int semk,int type, SYS_t systype):
            m_key(key),m_model(0),m_type(type),semkey(semk),s_type(systype),
        fd(-1){
            if(systype == PIPECLIENT)
            m_model = O_WRONLY|O_NONBLOCK;  //|O_NONBLOCK
            if(systype == PIPESERVER)
            m_model = O_RDONLY|O_NONBLOCK;
            //m_key
        }

        ~PdcPipe() {
        }
        typedef PdcPipe<T>* ptr;
			int Init(){
				int ret = m_Sem.Init(semkey);
				if (ret < 0) {
					m_sErrMsg.clear();
					m_sErrMsg = m_Sem.GetErrMsg();
					return ret;
				}
				if(1){
					if(access(m_key.c_str(), F_OK) != 0){
	                               ret = mkfifo(m_key.c_str(),  0777);
	                               if(ret < 0 ){
	                                 cerr<<"mkfifo :"<<m_key<<"failed "<<endl; 
						return -1; 
	                               }
					}
				}
				//if(s_type == PIPESERVER){
                           if(1){
                                 fd = ::open(m_key.c_str(), m_model);
					if(fd < 0 ){
					    cerr<<"open :"<<m_key<<"failed "<<endl; 
					    return fd; 
	                           }
				}
                          t = new T();
				assert(t);
				return 0;
			}
                    int ResetPipeKey(string newkey){
                        m_key.swap(newkey);
                        return 0;
                    };
                    int ResetSemKey(int newkey){
                         semkey = newkey; 
                         return 0;
                    }
                    const char * Getkeys() {return m_key.c_str();}
                    int GetSemKey() {return semkey;}
			int GetFd() {return fd;}
			bool isEmpty() {

				return false;
			}

			bool isFull() {

				return false;
			}

			int getSize() {

				return 0;
			}

                    int openpipe(){
                        if(fd != -1) return 0;
                        fd = ::open(m_key.c_str(), m_model);
                        if(fd < 0 ){
                           cerr<<"open m_key:"<<m_key <<" fd:"<<fd<<" failed"<<endl;
			       return -1;
                        }
                        return 0;
			}
			int  push(T *& a){
                        int r = 0;
                        //cerr<<"pipe to push:"<<m_key<<endl;
                        //semLockGuard oLock(m_Sem);
                        if(openpipe() < 0 ) return -1;
                        if (isFull()) {
                          return -1;
                        }
                        r = ::write(fd, a, sizeof(T));
                        //cerr<<" pipe push size:"<<r<<endl;
                        if(r != sizeof(T))
                            return -1;
						
                        return r;
			}

			T* pop() {
                        int r = 0;
                        //cerr<<"pipe to pop:"<<m_key<<endl;
                        //semLockGuard oLock(m_Sem);
                        if(openpipe() < 0 ) return NULL;
                        if (isEmpty()) {
                            return NULL;//应该选择抛出异常等方式，待改进；
                        }
                        //T *t = new T();
			    while(r  <= 0)		
                        r = ::read(fd, t ,sizeof(T));  //block
                        
                        if(r == sizeof(T)){
                            return t;
                        }
                        else
                            return NULL;
			}
			
                    void clear() { memset(t,0,sizeof(T));}
			std::string GetErrMsg() {
				return m_sErrMsg;
			}
		private:
			T *t;
			int fd;
			int semkey;
			string m_key;
			int m_type;
			char msg[PIPEMSGSIZE];
			SYS_t s_type;
			int m_model;
			SemLock m_Sem;
			std::string m_sErrMsg;
		};

	extern void copymqs(map<string ,void *> &mqs,PdcPipe<Msginfo>* send, 
	                                   PdcPipe<Msginfo>*recv);
	extern int createclientqueues(map<string ,void *> &mqs,bool sw);
	
	extern int createserverqueues(void * mqkeys,map<string ,void *> &mqs);

	}



//}

#endif 



