#pragma once
#ifndef _BOOSTMEMPOOL_HPP_
#define _BOOSTMEMPOOL_HPP_

#include <pthread.h>
#include <boost/pool/pool.hpp>
#include "type.h"
//#include <boost/pool/singleton_pool.hpp>

using namespace std;

/*this mempool is base-on boost::pool, but thead-safety */
template <typename T>
class BoostMemPool {
public:
	BoostMemPool():chunk_size(sizeof(T)), mem_pool(sizeof(T)) {
		pthread_mutex_init(&lock, NULL);
	}

	~BoostMemPool() {
		mem_pool.purge_memory();
	}
	
	inline T *malloc() {
		pthread_mutex_lock(&lock);
		//return (T *)mem_pool::malloc();
		T *p = (T *)mem_pool.malloc();
		//return  (T *)mem_pool.malloc();;
		pthread_mutex_unlock(&lock);
		return p;
	}
	
	inline void free(void *addr) {
		pthread_mutex_lock(&lock);
		//assert(is_from(addr));
		//mem_pool::free(addr);
		mem_pool.free(addr);
		pthread_mutex_unlock(&lock);
	}
	
	inline bool is_from(void *addr) {
		//return mem_pool::is_from(addr);
		return mem_pool.is_from(addr);
	}

	u32 get_chunk_size() const {
		return chunk_size;
	}
private:
	//bool init;
	//string pool_name;
	//u64 num_entry;
	//T *data;
	pthread_mutex_t lock;
	u32 chunk_size;
	//class MyPoolTag {};
	//typedef boost::singleton_pool<MyPoolTag, sizeof(T)> mem_pool;	
	boost::pool<> mem_pool;	
};

#endif

