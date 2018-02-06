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


static const uint32_t SHMSIZE = 1024*1024*1024;
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

/*
  we split the whole share memory to 3 chunks, as shown bellow:

           -------------------------------------------------------------
shm ->|   32k chunk(512MB)    |    128k chunk (512MB)     |    512k chunk(512MB)    |
           -------------------------------------------------------------
*/
#define SHM_SPLIT_CHUNK_NUM 3
#define SHM_CHUNK_SIZE_512M (512 * 1024 * 1024)
#define SHM_UNIT_SIZE_32K  (32 * 1024)
#define SHM_UNIT_SIZE_128K  (128 * 1024)
#define SHM_UNIT_SIZE_512K  (512 * 1024)

class ShmMem {
public:
	typedef unsigned int idx_type;
	
	enum {
		ALLOC_POLICY_SINGLE_UNIT = 1, 
		ALLOC_POLICY_MULTI_UNIT	= 2,
	};
	
	struct ShmChunkSize {
		u32 chunk_size;
		u32 unit_size;
	};

	struct ShmChunkDetail {
		u32 chunk_id;
		char *start_addr;
		u32 chunk_size;
		char *end_addr;
		idx_type *idx_array;
		u32 alloc_cursor;
		u32 num_unit;
		u32 min_unit_id;
		u32 max_unit_id;
		u32 num_free_unit;
		u32 unit_size;
		PdcLock lock;

		void dump() {
			cout << "part_id: " << chunk_id;
			cout << "start_addr: " << start_addr;
			cout << "chunk_size: " << chunk_size;
			cout << "alloc_cursor: " << alloc_cursor;
			cout << "num_unit: " << num_unit;
			cout << "min_unit_id: " << min_unit_id;
			cout << "max_unit_id: " << max_unit_id;
			cout << "num_free_unit: " << num_free_unit;
			cout << "unit_size: " << unit_size;
			cout << endl;
		}
	};
	
public:
    explicit ShmMem(int key,int iCreate=-1):m_pShm(NULL),m_iStatus(-1), m_iShmId(-1), 
        m_key(key),m_iCreate(iCreate),usetype(0),lock("shmlock"){
        m_pShm = NULL;
		shm_size = 0;
		alloc_policy = ALLOC_POLICY_SINGLE_UNIT;
		num_chunk = SHM_SPLIT_CHUNK_NUM;
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

		for (int i = 0; i < num_chunk; i++) {
			shm_size += chunk_size[i].chunk_size;
		}
		
        m_iShmId = ::shmget(m_key, shm_size, 0);
        if (m_iShmId < 0) {
            m_iCreate = 1;
            m_iShmId = ::shmget(m_key, shm_size, IPC_CREAT);
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
            if(usetype == 0) {
                cerr << "init to memory queue model" << endl;
                /*init shm part*/
				u32 last_chunk_size = 0;
				u32 last_chunk_max_unit_id = 0;

				for (u32 chunk_id = 0; chunk_id < num_chunk; chunk_id++) {
					ShmChunkDetail *chunk = &chunk_detail[chunk_id];
					u32 start_unit_id = 0;

					chunk->chunk_id = chunk_id;
					chunk->start_addr = m_pShm + last_chunk_size;
					chunk->chunk_size = chunk_detail[chunk_id].chunk_size;
					chunk->unit_size = chunk_detail[chunk_id].unit_size;
					chunk->num_unit = chunk->chunk_size / chunk->unit_size;
					chunk->idx_array = 
						(idx_type *)::malloc(part->num_unit * sizeof(idx_type));
					if (!chunk->idx_array) {
						cerr << "failed to malloc idx_array" << endl;
						assert(0);
					}
					
					start_unit_id = (last_chunk_max_unit_id == 0) ? 
									0 : (last_chunk_max_unit_id + 1);
					for (u32 i = 0; i < chunk->num_unit; i++) {
						u32 unit_id = i + start_unit_id;
						chunk->idx_array[i] = unit_id;
					}

					chunk->min_unit_id = chunk->idx_array[0];
					chunk->max_unit_id = chunk->idx_array[chunk->num_unit -1];
					chunk->alloc_cursor = chunk->num_unit - 1;
					chunk->num_free_unit = chunk->num_unit;

					last_chunk_size = chunk->chunk_size;
					last_chunk_max_unit_id = chunk->max_unit_id;
					chunk->dump();
				}
            }else if(usetype ==1){
            
            }
        }
        
        return m_iShmId;
    }

    bool isEmpty() {  //whether had memory to used?

		return (num_free_idx == 0);
	#if 0
	 if(sb->Avalid > 0)
            return false;
	 else if(sb->Avalid == 0)
	     return true;
	 else{
            cerr<<"superblock is:"<<sb<<endl;
            assert(0);
            return true;
	  }
	#endif
    }

    bool isFull() { // empty  == full ?

		return (num_free_idx == 0);
		#if 0
        if(sb->Avalid > 0)
            return false;
        else if(sb->Avalid == 0)
            return true;
        else{
            cerr<<"superblock is check full error:"<<sb<<endl;
            assert(0);
            return true;
        }
		#endif
    }

    int getSize() {

		return max_idx;
        //return sb->DataCount;
    }
    u64 getAvalid(){

		return num_free_idx;
        //return sb->Avalid;
    }
    /* if in one simple thread ,do not use lock
    *   but ,when use in multithreads ,a lock is needed.
    */
    int get(u32 size, u64* sum) {
		u32 last_chunk_unit_size = 0;
		u32 chunk_id;
	
		if (ALLOC_POLICY_SINGLE_UNIT == alloc_policy) { 
			for (chunk_id = 0 ; chunk_id < num_chunk; chunk_id++) {
				ShmChunkDetail *chunk = &chunk_detail[chunk_id];
				if (size > last_chunk_unit_size && size <= chunk->unit_size) {
					chunk->lock.lock();
					if (0 == chunk->num_free_unit) {
						last_chunk_unit_size = 0;
						chunk->lock.unlock();
						continue;
					}
					sum[0] = chunk->idx_array[chunk->alloc_cursor];
					chunk->alloc_cursor--;
					chunk->num_free_unit--;
					chunk->lock.unlock();
					return 1;
				}
				last_chunk_unit_size = chunk->unit_size;
			}

			if(num_chunk == chunk_id) {
				cerr << "failed to get share memory" << endl; 
				return -1;
			}
		}
	
    }

    T *getaddbyindex(u64 index){
		#if 0
        if(index  > sb->AllCount ){
            cerr<<"shm count max is:"<<sb->AllCount<<" now use:"<<index<<endl;;
        }
		#endif
        assert(index < max_idx);

        return (T *)(m_pShm + index * item_size);
    }
    int put(vector<u64> &used){
        //semLockGuard oLock(m_Sem);
        if (0 == used.size()) {
            cerr << "vector size is 0" << endl;
            return -2;
        }
		
        unsigned int size = 0;
        lock.lock();
        for (auto& it : used) {
            assert(it < max_idx);
            alloc_cursor++;
            idx_array[alloc_cursor] = it;
            //freelist.push_back(*it);
            size++;
        }
        num_free_idx += size;
        //freelist.assign(used.begin(), used.end());
        //freelist.splice(freelist.end(), used, used.begin(), used.end()); //for list
        //sb->Avalid  += size;
        //sb->Inuse -= size;
        //assert(sb->Inuse + sb->Avalid == sb->AllCount);
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
    //vector<u64> freelist;
    //vector<u64> usedlist;
    u64 *idx_array;
    u64 max_idx;
    u64 alloc_cursor;
    u64 item_size;
    u64 num_free_idx;
    char *pdata;
    T *pmem;
    SemLock m_Sem;
    PdcLock lock;
    std::string m_sErrMsg;

	u64 shm_size;
	u32 num_chunk;
	int alloc_policy;
	ShmChunkSize chunk_size[SHM_SPLIT_CHUNK_NUM] = {
		{SHM_CHUNK_SIZE_512M, SHM_UNIT_SIZE_32K}, 
		{SHM_CHUNK_SIZE_512M, SHM_UNIT_SIZE_128K},
		{SHM_CHUNK_SIZE_512M, SHM_UNIT_SIZE_512K},
	};
	ShmChunkDetail chunk_detail[SHM_SPLIT_CHUNK_NUM];
};


}
//}

#endif 



