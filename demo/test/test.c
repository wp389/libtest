#include <stddef.h>
#include <rbd/librbd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>


typedef long long int64;
typedef unsigned long long u64;

#define TEST_SIZE 1024
#define NAMESIZE 32
#define VOLUME "qemu-1"
#define POOL "wptest"


void rbd_aio_cb(rbd_completion_t c, void*arg)
{
    int r;
    
    r = rbd_aio_get_return_value( c);
    if(r<0) {
        printf("rbd_aio_cb ,return failed ,r= %u \n",r);
     
    }
    ///rbd_aio_release(c);

}

int main()
{
    rados_t cluster;
    rbd_image_t img;
    char *buf;
    int r;
    u64 offset, len;
    rados_ioctx_t ioctx;

    buf = (char *)malloc(TEST_SIZE);
    if(!buf) return -1;
    memset(buf, 6,TEST_SIZE);
    
    r = rados_create(&cluster ,NULL);
    if(r< 0) return -1;

    r = rados_conf_read_file(cluster, "/etc/ceph/ceph.conf");
    r = rados_connect(cluster);
    if(r< 0) return -1;

    r = rados_ioctx_create(cluster, POOL, &ioctx);
    if(r<0) return -1;

    r = rbd_open(ioctx, VOLUME, &img, NULL);
    if(r< 0) return -1;

    struct timeval start;
    struct timeval end;

   u64 retry = 10000;
   u64 n=0;
   len = TEST_SIZE;
   char test[] = "io qdpth =1";
   rbd_completion_t *c = (rbd_completion_t*)malloc(sizeof(rbd_completion_t) *(retry +1));
   gettimeofday(&start, NULL);
    while(retry){
        n++;
        r = rbd_aio_create_completion(test, (rbd_callback_t)rbd_aio_cb, &c[retry]);
        r = rbd_aio_write(img, n*2048, len, buf, c[retry]);
	 
       retry--;
    }
    rbd_aio_wait_for_complete( c[++retry]);
    gettimeofday(&end , NULL);
    printf("test io count: %u \n",n);
    printf("start at:%u s, %u us \n",start.tv_sec, start.tv_usec);
    printf("end at:%u s, %u us \n",end.tv_sec, end.tv_usec);

    return 0;
} 

