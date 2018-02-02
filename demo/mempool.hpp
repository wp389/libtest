#pragma once
#ifndef _MEMPOOL_HPP_
#define _MEMPOOL_HPP_

#include <pthread.h>
#include "type.h"

using namespace std;

/* 
             ---------------------------------------
pool ->  |        data      |idx |    data     |idx |...
             ---------------------------------------
             |<-datasize->|
             |<-   unitsize     ->|
             
             ---------------------------------------
unit_idx  ->   |idx|idx|idx|idx|idx|idx|...
             ---------------------------------------
			                        ^
			                         alloc_cursor  
*/
template <typename T>
class MemPool {
public:
	typedef unsigned int idx_type;
	struct Pool_Unit {
		T data;
		idx_type idx_ths_unit;
	};
		
	MemPool() {
        num_unit = MEMPOOL_UNIT_NUMER;
        //data_size = sizeof(T);
        unit_size = sizeof(T);
		idx_size = sizeof(idx_type);
		assert(unit_size == (data_size + idx_size));
		
        pool = (T *)::malloc(num_unit * unit_size);
        if (!pool) {
            cout << "failed to malloc pool memory" << endl;
			assert(0);
        }	
		//for (idx_type i = 0; i < num_unit; i++) {
		//	pool[i].idx_ths_unit = i;
		//}
		pool_start = (void *)pool;
		pool_end = (void *)((char *)pool + num_unit * unit_size);
		
		unit_idxs = (idx_type *)::malloc(num_unit * idx_size);
		if (!unit_idxs) {
            cout << "failed to malloc unit index" << endl;
			assert(0);
		}
		for (idx_type i = 0; i < num_unit; i++) {
			unit_idxs[i] = i;
		}

		alloc_cursor = num_unit - 1;
		free_unit = num_unit;
		pthread_mutex_init(&lock, NULL);
		cout << "num_unit " << num_unit << " data_size " << data_size <<
			  " unit_size " << unit_size << " alloc_cursor " << alloc_cursor <<
			  " free_unit " << free_unit << " pool_start " << pool_start <<
			  " pool_end " << pool_end <<endl;
	}

	~MemPool() {
		if (pool) {
			::free(pool);
			pool = NULL;
		}

		if (unit_idxs) {
			::free(unit_idxs);
			unit_idxs = NULL;
		}
	}
	
	inline T *malloc() {
		pthread_mutex_lock(&lock);
		if (0 == free_unit) {
			pthread_mutex_unlock(&lock);
			return NULL;
		}		
		idx_type unit_idx = unit_idxs[alloc_cursor];
		--alloc_cursor;
		--free_unit;
		pthread_mutex_unlock(&lock);
		//assert(pool[unit_idx].idx_ths_unit == unit_idx);
		return (T *)&pool[unit_idx];
	}
	
	inline void free(void *addr) {
		assert(is_from(addr));
		//Pool_Unit *unit = (Pool_Unit *)addr;
		idx_type unit_idx = addr;
		assert(unit_idx < num_unit);
		pthread_mutex_lock(&lock);
		++alloc_cursor;
		unit_idxs[alloc_cursor] = unit_idx;
		++free_unit;
		pthread_mutex_unlock(&lock);
	}
	
	inline bool is_from(void *addr) {
		return (addr >= pool_start && addr < pool_end);
	}

	u32 get_unit_size() const {
		return unit_size;
	}

	u32 get_pool_size() const {
		return unit_size * num_unit;
	}
private:
	pthread_mutex_t lock;
    Pool_Unit *pool;
	void *pool_start;
	void *pool_end;
    idx_type *unit_idxs;
	u32 alloc_cursor;
    u32 pool_size;
	u32 unit_size;
	u32 data_size;
	u32 idx_size;
	u32 num_unit;
    u32 max_index;
	u32 free_unit;
};

#endif

