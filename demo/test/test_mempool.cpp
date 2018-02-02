#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include <vector>
#include "../mempool.hpp"
#include "../boost_mempool.hpp"

using namespace std;

#define THREAD_NUM 2
#define TEST_TIMES 1000
#define ALLOC_NUM 50000
#define ALLOC_SIZE 100
#define US_PER_SEC 1000000ul
#define MS_PER_SEC 1000
enum {
	NEW_ALLOC = 1,
	BOOSTPOOL_ALLOC = 2,
	MYTPOOL_ALLOC = 3,
};

struct MyType {
	int _a;
	int _b;
	char _c[ALLOC_SIZE];
};

BoostMemPool<MyType> boost_pool;
MemPool<MyType> my_pool;

static void *alloc_func(void *arg) {
	//boost::pool<> pool(sizeof(struct MyType));
	struct MyType *boostpool_objs[ALLOC_NUM];
	struct MyType *mypool_objs[ALLOC_NUM];
	struct MyType *new_objs[ALLOC_NUM];
	struct timeval start;
    struct timeval end;
	float time_used;
	int tid;

	tid = pthread_self();
	printf("my thread is %u\n", tid);

	gettimeofday(&start, NULL);
	if (NEW_ALLOC == (*(int *)arg)) {
		printf("do new alloc\n");
		for (int i = 0; i < TEST_TIMES; i++) {
				for (int j = 0; j < ALLOC_NUM; j++) {
					MyType *p = new MyType();
					p->_a = j;
					p->_b = 333 + i;
					new_objs[j] = p;
				}
		
				for (int k = 0; k < ALLOC_NUM; ++k) {
					assert(((struct MyType *)new_objs[k])->_b == (333 + i));
					delete new_objs[k];
					new_objs[k] = NULL;
				}
		}
	} else if (BOOSTPOOL_ALLOC == (*(int *)arg)) {
		printf("do boost alloc\n");
		for (int i = 0; i < TEST_TIMES; i++) {
				for (int j = 0; j < ALLOC_NUM; j++) {
					MyType *p = (struct MyType *)boost_pool.malloc();
					p->_a = j;
					p->_b = (444 + i);
					boostpool_objs[j] = p;
					//printf("1111111111\n");
				}
		
				for (int k = 0; k < ALLOC_NUM; k++) {
					assert(((struct MyType *)boostpool_objs[k])->_b == (444+ i));
					boost_pool.free(boostpool_objs[k]);
					boostpool_objs[k] = NULL;
				}
				//printf("2222222222\n");
		}
	} else if (MYTPOOL_ALLOC == (*(int *)arg)) {
		for (int i = 0; i < TEST_TIMES; i++) {
				for (int j = 0; j < ALLOC_NUM; j++) {
					MyType *p = (struct MyType *)my_pool.malloc();
					//printf("1111111111\n");
					p->_a = j;
					p->_b = (555 + i);
					mypool_objs[j] = p;
					//printf("1111111111\n");
				}
		
				for (int k = 0; k < ALLOC_NUM; k++) {
					assert(((struct MyType *)mypool_objs[k])->_b == (555+ i));
					my_pool.free(mypool_objs[k]);
					//printf("2222222222\n");
					mypool_objs[k] = NULL;
				}
				//printf("2222222222\n");
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
	int alloc_type;

	printf("---test new---\n");
	alloc_type = NEW_ALLOC; 
	for (int i = 0; i < THREAD_NUM; i++) {
		ret = pthread_create(&t[i], NULL, alloc_func, (void *)&alloc_type);
		if (ret != 0) {
			printf("thread-%d create failed\n", i);
			return ret;
		}
	}

	for (int i = 0; i < THREAD_NUM; i++) {
		pthread_join(t[i], NULL);
	}
	
	printf("sleep 5sec \n");
	sleep(1);	
	
	printf("---test boostpool---\n");
	alloc_type = BOOSTPOOL_ALLOC; 
	for (int i = 0; i < THREAD_NUM; i++) {
		ret = pthread_create(&t[i], NULL, alloc_func, (void *)&alloc_type);
		if (ret != 0) {
			printf("thread1 init failed\n");
			return ret;
		}
	}
	
	for (int i = 0; i < THREAD_NUM; i++) {
		pthread_join(t[i], NULL);
	}
	
	printf("sleep 5sec \n");
	sleep(1);	
	#if 1
	printf("---test mytpool---\n");
	alloc_type = MYTPOOL_ALLOC; 
	for (int i = 0; i < THREAD_NUM; i++) {
		ret = pthread_create(&t[i], NULL, alloc_func, (void *)&alloc_type);
		if (ret != 0) {
			printf("thread1 init failed\n");
			return ret;
		}
	}
	
	for (int i = 0; i < THREAD_NUM; i++) {
		pthread_join(t[i], NULL);
	}
	#endif

	return 0;
}
	
	
