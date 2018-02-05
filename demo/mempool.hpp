#pragma once
#ifndef _MEMPOOL_HPP_
#define _MEMPOOL_HPP_

#include <pthread.h>
#include "type.h"

using namespace std;

/*         <-  unitsize ->
             -------------------------------------------
pool ->  |        data     |    data     |    data     |    data     |
             -------------------------------------------
            ^                                                    ^
		pool_start                                        pool_end
             
                      -------------------------------------
idx_array  ->   |idx|idx|idx|idx|idx|...
                      -------------------------------------
			                          ^
			                         alloc_cursor  
*/
template <typename T>
class MemPool {
public:
	typedef unsigned int idx_type;
		
	MemPool() {
        num_unit = MEMPOOL_UNIT_NUMER;
        unit_size = sizeof(T);
		idx_size = sizeof(idx_type);
		
        pool = (T *)::malloc(num_unit * unit_size);
        if (!pool) {
            cout << "failed to malloc pool" << endl;
			assert(0);
        }	
		//for (idx_type i = 0; i < num_unit; i++) {
		//	pool[i].idx_ths_unit = i;
		//}
		pool_start = (char *)pool;
		pool_end = (char *)((char *)pool + (num_unit - 1) * unit_size);
		
		idx_array = (idx_type *)::malloc(num_unit * idx_size);
		if (!idx_array) {
            cout << "failed to malloc idx_array" << endl;
			assert(0);
		}
		for (idx_type i = 0; i < num_unit; i++) {
			idx_array[i] = i;
		}

		alloc_cursor = num_unit - 1;
		num_free_unit = num_unit;
		pthread_mutex_init(&lock, NULL);
		//cout << "num_unit " << num_unit  <<
		//	  " unit_size " << unit_size << " alloc_cursor " << alloc_cursor <<
		//	  " free_unit " << num_free_unit << " pool_start " << pool_start <<
		//	  " pool_end " << pool_end <<endl;
	}

	~MemPool() {
		if (pool) {
			//cout << "free pool" << endl;
			::free(pool);
			pool = NULL;
		}

		if (idx_array) {
			//cout << "free idx_array" << endl;
			::free(idx_array);
			idx_array = NULL;
		}
	}
	
	inline T *malloc() {
		pthread_mutex_lock(&lock);
		if (0 == num_free_unit) {
			pthread_mutex_unlock(&lock);
			return NULL;
		}		
		idx_type unit_idx = idx_array[alloc_cursor];
		--alloc_cursor;
		--num_free_unit;
		pthread_mutex_unlock(&lock);
		//assert(pool[unit_idx].idx_ths_unit == unit_idx);
		return (T *)&pool[unit_idx];
	}
	
	inline void free(void *addr) {
		assert(is_from(addr));
		idx_type unit_idx = ((u64)addr - (u64)pool_start) / unit_size;
		assert(unit_idx < num_unit);
		pthread_mutex_lock(&lock);
		++alloc_cursor;
		idx_array[alloc_cursor] = unit_idx;
		++num_free_unit;
		pthread_mutex_unlock(&lock);
	}
	
	inline bool is_from(void *addr) {
		return ((char *)addr >= pool_start && (char *)addr <= pool_end);
			    ((unit_size % ((u64)addr + unit_size - (u64)pool_start)) == 0);
	}

	u32 get_unit_size() const {
		return unit_size;
	}

	u32 get_pool_size() const {
		return unit_size * num_unit;
	}
private:
	pthread_mutex_t lock;
    T *pool;
	char *pool_start;
	char *pool_end;
    idx_type *idx_array;
	u32 alloc_cursor;
    u32 pool_size;
	u32 unit_size;
	u32 idx_size;
	u32 num_unit;
	u32 num_free_unit;
};

#endif

