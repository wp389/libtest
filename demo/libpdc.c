//g++ msserver.c -o server
//g++ msclient.c -o client
#include "type.h"



extern pdc_rados_t pdc_rados_create(rados_t *cluster, const char * const id);

extern int pdc_rados_connect();

extern pdc_rados_t pdc_rados_create_ioctx(pdc_rados_t rados, pdc_rados_ioctx_t &ioctx);

extern int pdc_rbd_open(pdc_rados_ioctx_t ioctx, const char * imagename, rbd_image_t image, int snap_id);


pdc_rados_t pdc_rados_create(pdc_rados_t *cluster, const char * const id)
{
    cerr<<" do create rados"<<cluster <<endl;
    pdc_rados_t pdc_rados = new Pdc_rados(); 

}
int pdc_rados_connect();

int pdc_rbd_open();




