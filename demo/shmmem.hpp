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


#if 0
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
#endif
/*
we split the whole share memory to several chunks, for example:

           ----------------------------------------------------------------
shm ->| chunk 512MB,32K unit | chunk 512MB,128K unit | chunk 512MB,512K unit  |
           ----------------------------------------------------------------

                         -------------------------------------------------------
chunk(512MB) -> | 32k unit |32k unit |32k unit |32k unit |32k unit |...                |
                         -------------------------------------------------------
                         ^
                          start_addr
*/       
#define SHM_SPLIT_CHUNK_NUM 4
#define SHM_CHUNK_SIZE_512M (512 * 1024 * 1024)
#define SHM_UNIT_SIZE_32K  (32 * 1024)
#define SHM_UNIT_SIZE_128K  (128 * 1024)
#define SHM_UNIT_SIZE_512K  (512 * 1024)
#define SHM_UNIT_SIZE_4M  (4* 1024 * 1024)


class ShmMem {
public:
    typedef unsigned int idx_type;
	
    enum {
        ALLOC_POLICY_SINGLE_UNIT = 0, 
        ALLOC_POLICY_MULTI_UNIT	= 1,
    };

    enum {
        SHM_USE_TYPE_CLIENT = 0,
        SHM_USE_TYPE_SERVER = 1,
    };
	
    struct ShmChunkSize {
        u32 chunk_size;
        u32 unit_size;
    };
 
    struct ShmChunkDetail {
        ShmChunkDetail():start_addr(NULL), end_addr(NULL),
			idx_array(NULL),lock("shm_lock") {}
        ~ShmChunkDetail() {
            if (idx_array) {
                ::free(idx_array);
				idx_array = NULL;
            }
        }			
        void dump() {
            cout << "chunk_id: " << chunk_id;
            cout << " start_addr: " << std::hex << (u64)start_addr;
            cout << std::dec;
            cout << " chunk_size: " << chunk_size;
            cout << " unit_size: " << unit_size;
            cout << " alloc_cursor: " << alloc_cursor;
            cout << " min_unit_id: " << min_unit_id;
            cout << " max_unit_id: " << max_unit_id;
            cout << " num_unit: " << num_unit;
            cout << " num_free_unit: " << num_free_unit;
            cout << endl;
        }
		
        u32 chunk_id;
        char *start_addr;
        char *end_addr;
        idx_type *idx_array;
        u32 chunk_size;
        u32 unit_size;
        u32 alloc_cursor;
        u32 min_unit_id;
        u32 max_unit_id;
        u32 num_unit;
        u32 num_free_unit;
        PdcLock lock;
    };
	
public:
    explicit ShmMem(int key,int iCreate=-1):m_pShm(NULL),m_iStatus(-1), m_iShmId(-1), 
        m_key(key),m_iCreate(iCreate),lock("shmlock"){
        m_pShm = NULL;
        shm_size = 0;
        alloc_policy = ALLOC_POLICY_SINGLE_UNIT;
        num_chunk = SHM_SPLIT_CHUNK_NUM;
        if (1 == iCreate) {
            use_type = SHM_USE_TYPE_SERVER;
        } else {
            use_type = SHM_USE_TYPE_CLIENT;
        }			
    }

    ~ShmMem() {
        ::shmdt(m_pShm);
        for (u32 chunk_id = 0; chunk_id < num_chunk; chunk_id++) {
            ShmChunkDetail *chunk = &(chunk_detail[chunk_id]);
            if (chunk->idx_array) {
                ::free(chunk->idx_array);
                chunk->idx_array = NULL;
            }
        }
    }
    int Init(){
        int ret;
		#if 0
        int ret = m_Sem.Init(m_key);
        if (ret < 0) {
            m_sErrMsg.clear();
            m_sErrMsg = m_Sem.GetErrMsg();
            return ret;
        }
		#endif
        /*get share memroy size*/
        for (int i = 0; i < num_chunk; i++) {
            shm_size += chunk_size[i].chunk_size;
        }
		
        m_iShmId = ::shmget(m_key, shm_size, 0777);
        if (m_iShmId < 0) {
            m_iCreate = 1;
            m_iShmId = ::shmget(m_key, shm_size, IPC_CREAT| 0777);
        }
		
        if (m_iShmId < 0) {
            m_sErrMsg.clear();
            m_sErrMsg = "shmget error ";
            m_iStatus = SHMQUEUEERROR;
            return m_iShmId;
        }

        m_pShm = (char*)::shmat(m_iShmId,NULL,0);//读写模式；
        if (m_pShm == (void *)-1) {
            m_sErrMsg.clear();
            m_sErrMsg = "shmat error ";
            return -1;
        }
                
        ret = init_chunk_detail(use_type);		
        if (ret < 0) {
            m_sErrMsg.clear();
            m_sErrMsg = "failed to init chunk detail ";
            return -1;
        }
        return m_iShmId;
    }

#if 0
    bool isEmpty() {  //whether had memory to used?

		return (num_free_idx == 0);
    }

    bool isFull() { // empty  == full ?

		return (num_free_idx == 0);
    }

    int getSize() {

		return max_idx;
    }
    u64 getAvalid(){

		return num_free_idx;
    }
#endif
    /* if in one simple thread ,do not use lock
    *   but ,when use in multithreads ,a lock is needed.
    */
    int get(u32 size, u64 *sum) {
        assert(use_type == SHM_USE_TYPE_SERVER);
        u32 last_chunk_unit_size = 0;
		u32 chunk_id;
	
		if (ALLOC_POLICY_SINGLE_UNIT == alloc_policy) { 
			for (chunk_id = 0 ; chunk_id < num_chunk; chunk_id++) {
				ShmChunkDetail *chunk = &(chunk_detail[chunk_id]);
				if (size > last_chunk_unit_size && size <= chunk->unit_size) {
					chunk->lock.lock();
					if (0 == chunk->num_free_unit) {
						last_chunk_unit_size = 0;
						chunk->lock.unlock();
						continue;
					}
					sum[0] = chunk->idx_array[chunk->alloc_cursor];
                    assert(sum[0] >= chunk->min_unit_id && 
						       sum[0] <= chunk->max_unit_id);
					chunk->alloc_cursor--;
					chunk->num_free_unit--;
					chunk->lock.unlock();
					return 1;
				}
				last_chunk_unit_size = chunk->unit_size;
			}

			if (num_chunk == chunk_id) {
				cerr << "failed to get share memory" << endl; 
				return -1;
			}
		}
    }

    int put(vector<u64> &used) {
		assert(use_type == SHM_USE_TYPE_SERVER);
        if (0 == used.size()) {
            cerr << "vector size is 0" << endl;
            return -2;
        }
        int put_size = 0;
        int chunk_id;
        for (auto& it : used) {
            chunk_id = get_chunk_id(it);
            if (-1 == chunk_id) {
                cerr << "invalid memory index" << endl;
                continue;
            }
            ShmChunkDetail *chunk = &(chunk_detail[chunk_id]);
            assert(it >= chunk->min_unit_id && it <= chunk->max_unit_id);
            chunk->lock.lock();
            chunk->alloc_cursor++;
            chunk->idx_array[chunk->alloc_cursor] = it;
            chunk->num_free_unit++;
            chunk->lock.unlock();
            put_size++;
        }
        
        return put_size;
    }
	
    void *getaddbyindex(const u64 index) const {
        //assert(use_type == SHM_USE_TYPE_CLIENT);
        int chunk_id;

        chunk_id = get_chunk_id(index);
        if (-1 == chunk_id) {
            cerr << "invalid memory index" << endl;
            return NULL;
        }
        ShmChunkDetail const *chunk = &(chunk_detail[chunk_id]);
        assert(index >= chunk->min_unit_id && index <= chunk->max_unit_id);
        return (void *)(chunk->start_addr + 
			           (index - chunk->min_unit_id) * chunk->unit_size);
    }

	int get_unit_size(const u64 index) const {
        //assert(use_type == SHM_USE_TYPE_CLIENT);
        int chunk_id;

        chunk_id = get_chunk_id(index);
        if (-1 == chunk_id) {
            cerr << "invalid memory index" << endl;
            return -1;
        }
        ShmChunkDetail const *chunk = &(chunk_detail[chunk_id]);
        return chunk->unit_size;
	}

    int init_chunk_detail(int type) {        
        u32 prev_chunk_size = 0;
        u32 last_chunk_max_unit_id = 0;
        u32 start_unit_id = 0;

        if (!m_pShm) {
            cerr << "error, m_pShm is NULL" << endl;
            return -1;
        }			
        for (u32 chunk_id = 0; chunk_id < num_chunk; chunk_id++) {
            ShmChunkDetail *chunk = &(chunk_detail[chunk_id]);
            chunk->chunk_id = chunk_id;
            chunk->start_addr = m_pShm + prev_chunk_size;
            chunk->chunk_size = chunk_size[chunk_id].chunk_size;
            chunk->unit_size = chunk_size[chunk_id].unit_size;
            chunk->num_unit = chunk->chunk_size / chunk->unit_size;
			
            start_unit_id = (last_chunk_max_unit_id == 0) ? 
							0 : (last_chunk_max_unit_id + 1);
            chunk->min_unit_id = start_unit_id;
            chunk->max_unit_id = (chunk->min_unit_id + chunk->num_unit - 1);
            if (SHM_USE_TYPE_SERVER == type) {
                cerr << "shm used by server" << endl;
                chunk->idx_array = 
                    (idx_type *)::malloc(chunk->num_unit * sizeof(idx_type));
                if (!chunk->idx_array) {
                    cerr << "failed to malloc idx_array" << endl;
					//TODO:release other chunk memory
				    return -1;
                }
                u32 unit_id = 0;
                for (u32 i = 0; i < chunk->num_unit; i++) {
                    unit_id = i + start_unit_id;
                    chunk->idx_array[i] = unit_id;
                }

                chunk->alloc_cursor = chunk->num_unit - 1;
                chunk->num_free_unit = chunk->num_unit;
            } else {
                cerr << "shm used by client" << endl;
                chunk->idx_array = NULL;
                chunk->alloc_cursor = 0;
                chunk->num_free_unit = 0;
            }
		
            prev_chunk_size += chunk->chunk_size;
            last_chunk_max_unit_id = chunk->max_unit_id;
            chunk->dump();
        }
		return 0;
    }

    int get_chunk_id(const idx_type index) const {		
		int ret_chunk_id = -1;

        for (idx_type chunk_id = 0; chunk_id < num_chunk; chunk_id++) {
            ShmChunkDetail const *chunk = &(chunk_detail[chunk_id]);
            if (index >= chunk->min_unit_id &&
                index <= chunk->max_unit_id) {
                ret_chunk_id = chunk_id;
                break;
            }
        }
		
        return ret_chunk_id;
    }

    std::string GetErrMsg() {
        return m_sErrMsg;
    }
private:
    //SuperBlock*sb;
    //int usetype;
    char *m_pShm;
    int m_iShmId;
    int m_iStatus;
    int m_iCreate;
    int m_key;
    //char *pdata;
    //T *pmem;
    //SemLock m_Sem;
    PdcLock lock;
    std::string m_sErrMsg;

    int use_type;
    int num_chunk;
    int alloc_policy;
    u64 shm_size;
    ShmChunkSize chunk_size[SHM_SPLIT_CHUNK_NUM] = 
    {
        {SHM_CHUNK_SIZE_512M, SHM_UNIT_SIZE_32K}, 
        {SHM_CHUNK_SIZE_512M, SHM_UNIT_SIZE_128K},
        {SHM_CHUNK_SIZE_512M, SHM_UNIT_SIZE_512K},
        {SHM_CHUNK_SIZE_512M, SHM_UNIT_SIZE_4M},
    };
    ShmChunkDetail chunk_detail[SHM_SPLIT_CHUNK_NUM];
};


}
//}

#endif 



