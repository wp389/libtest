#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include <vector>
#include "../mempool.hpp"
#include <boost/pool/pool.hpp>

using namespace std;

#define THREAD_NUM 4
#define ALLOC_NUM 1000
#define TEST_TIMES 50000
#define ALLOC_SIZE 512
#define US_PER_SEC 1000000ul
#define MS_PER_SEC 1000
#define MEMPOOL_ALLOC 0
#define NEW_ALLOC 1

struct MyType {
	int _a;
	int _b;
	char _c[ALLOC_SIZE];
};

MemPool<MyType> pool;

static void *alloc_func(void *arg) {
	//boost::pool<> pool(sizeof(struct MyType));
	struct MyType *mempool_objs[ALLOC_NUM];
	struct MyType *new_objs[ALLOC_NUM];
	struct timeval start;
    struct timeval end;
	float time_used;
	int tid;

	tid = pthread_self();
	printf("my thread is %u\n", tid);

	gettimeofday(&start, NULL);

	if (MEMPOOL_ALLOC == (*(int *)arg)) {
		for (int j = 0; j < TEST_TIMES; j++) {
				for (int k = 0; k < ALLOC_NUM; k++) {
					MyType *p = (struct MyType *)pool.malloc();
					p->_a = k;
					p->_b = (222 + j);
					mempool_objs[k] = p;
					//printf("1111111111\n");
				}
		
				for (int i = 0; i < ALLOC_NUM; ++i) {
					assert(((struct MyType *)mempool_objs[i])->_b == (222 + j));
					pool.free(mempool_objs[i]);
					mempool_objs[i] = NULL;
				}
				//printf("2222222222\n");
		}
	} else if (NEW_ALLOC == (*(int *)arg)) {
		for (int j = 0; j < TEST_TIMES; j++) {
				for (int k = 0; k < ALLOC_NUM; k++) {
					MyType *p = new MyType();
					p->_a = k;
					p->_b = 333 + j;
					new_objs[k] = p;
				}
		
				for (int i = 0; i < ALLOC_NUM; ++i) {
					assert(((struct MyType *)new_objs[i])->_b == (333 + j));
					delete new_objs[i];
					new_objs[i] = NULL;
				}
		}
	} else {
		assert(0);
	}
	gettimeofday(&end, NULL);
	time_used = (end.tv_sec - start.tv_sec) * US_PER_SEC + (end.tv_usec - start.tv_usec);
	time_used /= MS_PER_SEC;
	printf("tid:%u, %s start at:%u s, %u us \n", tid, 
		((*(int *)arg) == NEW_ALLOC) ? "new" : "mempool", start.tv_sec, start.tv_usec);
	printf("tid:%u, %s end at:%u s, %u us \n", tid, 
		((*(int *)arg) == NEW_ALLOC) ? "new" : "mempool", end.tv_sec, end.tv_usec);
	printf("tid:%u, time_used:%f ms\n", tid, time_used);

	return NULL;
}

int main() {
	//boost::pool<> pool(sizeof(struct MyType));
	pthread_t t[THREAD_NUM];
	int ret;
	int mempool_alloc = MEMPOOL_ALLOC;
	int new_alloc = NEW_ALLOC; 

	printf("---test mempool---\n");
	for (int i = 0; i < THREAD_NUM; i++) {
		ret = pthread_create(&t[i], NULL, alloc_func, (void *)&mempool_alloc);
		if (ret != 0) {
			printf("thread-%d create failed\n", i);
			return ret;
		}
	}

	for (int i = 0; i < THREAD_NUM; i++) {
		pthread_join(t[i], NULL);
	}
	
	printf("sleep 5sec \n");
	sleep(5);	
	
	printf("---test new---\n");
	for (int i = 0; i < THREAD_NUM; i++) {
		ret = pthread_create(&t[i], NULL, alloc_func, (void *)&new_alloc);
		if (ret != 0) {
			printf("thread1 init failed\n");
			return ret;
		}
	}
	
	for (int i = 0; i < THREAD_NUM; i++) {
		pthread_join(t[i], NULL);
	}
	
	return 0;
}
	
	
