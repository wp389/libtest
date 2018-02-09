//g++ msserver.c -o server
//g++ msclient.c -o client
#include "type.h"
#include <iostream>
#include <sstream>

#include "librbd.hpp"
#include "pdcclient.hpp"

PdcClient *pdc_client_mgr;


extern "C" void rbd_version(int *major, int *minor, int *extra)
{
  if (major)
    *major = LIBRBD_VER_MAJOR;
  if (minor)
    *minor = LIBRBD_VER_MINOR;
  if (extra)
    *extra = LIBRBD_VER_EXTRA;
}

extern "C" int rbd_resize(rbd_image_t image, uint64_t size)
{
    return 0;
}
extern "C" int rbd_create(rados_ioctx_t p, const char *name, uint64_t size, int *order)
{
    printf("rbd_create \n");
    return 0;
}
extern "C" int rados_create(rados_t *prados)
//int pdc_create_rados(pdc_rados_t *prados)
{
    int r;
    PdcClient * vmclient;
    if(!pdc_client_mgr){
        vmclient = new PdcClient("vm");
        pdc_client_mgr  = vmclient;
        assert(vmclient);
        r = vmclient->init();
        if(r < 0){
            cerr<<"client init failed :"<< r <<endl;
            return -1;
        }
    }else{
         vmclient = pdc_client_mgr;
    }
    vmclient->inc_ref();
    *prados = vmclient;
    return 0;
}

extern "C" int rados_conf_read_file(rados_t cluster, const char *path_list)
//int pdc_rados_conf_read_file(pdc_rados_t vmclient, const char * path)
{
    int r =0;
    //todo:
    // to tell server which file to read;
    
    return r ;
}

extern "C" int rados_conf_set(rados_t cluster, const char *option, const char *value)
//int pdc_rados_conf_set(pdc_rados_t cluster, const char *option, const char *value)
{
    cerr<<"pdc_rados_conf_set is null now"<<endl;
    return 0;
}

extern "C" int rados_connect(rados_t cluster)
//int pdc_connect_rados(pdc_rados_t vmclient)
{
    PdcClient * pclient = reinterpret_cast<PdcClient *>(cluster);
    pclient->inc_ref();
    cerr<<"pdc_connect_rados "<<endl;
    return 0;
}

extern "C" int rados_ioctx_create(rados_t vmclient, const char *pool_name,
//int pdc_rados_ioctx_create(pdc_rados_t vmclient, const char *pool_name,
                                  rados_ioctx_t *ioctx)
{
    int r;
    BackendClient::RadosClient *prados;
    string radosname(pool_name);
    PdcClient * pclient = reinterpret_cast<PdcClient *>(vmclient);
    
    BackendClient *pceph = pclient->clusters["ceph"];
    if(!pceph){
        cerr<<"ceph rados ioctx is null ,in pid:"<<getpid()<<endl;
        return -1;
    }
    map<string ,BackendClient::RadosClient *>::iterator it = pceph->radoses.find(radosname);
    if(it == pceph->radoses.end()){  // not exist ,create a new one
        prados = new BackendClient::RadosClient(radosname, "/etc/ceph/ceph.conf",pceph);
        prados->init(0);
        pceph->radoses[radosname] = prados;
            
        Msginfo *msg = new Msginfo();
        msg->opcode = OPEN_RADOS;
        strcpy(msg->u.mgr.client.cluster,"ceph");
        strcpy(msg->u.mgr.client.pool,pool_name);
    
        strcpy(msg->u.mgr.mqkeys.key , pclient->ackmq->Getkeys());
        msg->u.mgr.mqkeys.semkey = pclient->ackmq->GetSemKey();
        if(MULTIPIPE){
            strcpy(msg->u.mgr.mqkeys.recvkey, pclient->sendmq->Getkeys());
            msg->u.mgr.mqkeys.recvsem = pclient->sendmq->GetSemKey();  //client's send is server's recv
        }else{
            strcpy(msg->u.mgr.mqkeys.recvkey, pclient->msgmq.Getkeys());
            msg->u.mgr.mqkeys.recvsem = pclient->msgmq.GetSemKey();  //client's send is server's recv
        }
        msg->dump("open rados");
        r = pclient->msgmq.push(msg);
        delete msg;
        if(r<  0 ){
            cerr<<" create remote rados failed"<<endl;
            return -1;
        }
    }else{
    prados = pceph->radoses[radosname];
    assert(prados);
    if(!prados){
        cerr<<"create prados_ioctx failed "<<endl;
        return -1;
    }
    }
    *ioctx = (void *)prados;
    return 0;
}

extern "C" void rados_ioctx_destroy(rados_ioctx_t io)
//void pdc_rados_ioctx_destroy(pdc_rados_ioctx_t io)
{
    cerr<<"pdc_rados_ioctx_destroy is null now"<<endl;

    
}


extern "C" void rados_shutdown(rados_t cluster)
//void pdc_rados_shutdown(pdc_rados_t cluster)
{
    cerr<<"pdc_rados_shutdown is null now"<<endl;
    return ;
}

extern "C" ssize_t rbd_aio_get_return_value(rbd_completion_t c)
//ssize_t pdc_rbd_aio_get_return_value(rbd_completion_t c)
{
    PdcCompletion *comp = (PdcCompletion *)c;
    return comp->get_return_value(); 
}

extern "C" int rbd_open(rados_ioctx_t ioctx, const char *rbd_name, rbd_image_t *image,
			const char *snap_name)
//int pdc_rbd_open(rados_ioctx_t ioctx,rbd_image_t * image,const char * rbd_name)
{
    int r;
    string pkey;
    int skey = 0;
    string rbdname(rbd_name);
    string poolname;
    PdcClient *pdcclient = pdc_client_mgr;

    BackendClient::RbdVolume*prbd;   
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
    //PdcClient * pclient = pdc_client_mgr;
    BackendClient::RadosClient *prados = reinterpret_cast<BackendClient::RadosClient *>(ioctx);

    if(prados->volumes.find(rbdname) != prados->volumes.end()){  //exist
        cerr<<"find pdb exist"<<endl;
        prbd = reinterpret_cast<BackendClient::RbdVolume*>(prados->volumes[rbdname]);
        *image = (void *)prbd;
    }else {    //NOT EXIST
        prbd = new BackendClient::RbdVolume(rbdname, prados);
        prbd->init(0);
        if(MULTIPIPE)
            pdcPipe::copymqs(prbd->mq, pdcclient->sendmq,pdcclient->ackmq);     
        else
            pdcPipe::copymqs(prbd->mq, &pdcclient->msgmq,pdcclient->ackmq);
        r = prbd->init(0);
        if(r< 0){
            cerr<<"client create new rbd:"<<rbdname<<" failed :"<<r<<endl;
            return -1;
        }
        *image = (void *)prbd;
        prados->volumes[rbdname] = (void *)prbd;
		
    pdcPipe::PdcPipe<Msginfo>::ptr recvmq = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(prbd->mq[RECVMQ]);
    pdcPipe::PdcPipe<Msginfo>::ptr sendmq = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(prbd->mq[SENDMQ]);
    //msginfo will change to msgpool list  next step
    Msginfo *msg = new Msginfo();
    msg->opcode = OPEN_RBD;
    
    strcpy(msg->u.mgr.client.cluster,"ceph");
    strcpy(msg->u.mgr.client.pool, prados->GetName());
    strcpy(msg->u.mgr.client.volume,rbd_name);
    // init client recvmq : server sendmq
    strcpy(msg->u.mgr.mqkeys.key,recvmq->Getkeys());  //client recv pipe
    msg->u.mgr.mqkeys.semkey = recvmq->GetSemKey();

    //init client sendmq : server recvmq
    strcpy(msg->u.mgr.mqkeys.recvkey,sendmq->Getkeys());  //client send pipe
    msg->u.mgr.mqkeys.recvsem= sendmq->GetSemKey();
    msg->dump("open rbd");

    //if(!MULTIPIPE)
    p_pipe = &pdcclient->msgmq;   // use public to send register info
    r = p_pipe->push(msg);
    delete msg;
    if(r<  0 ){
        cerr<<" create remote rados failed"<<endl;
        return -1;
    }
    cerr<<"send over and wait for open pipes"<<endl;
    //todo : send zero meg to active remote pipe.
    
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
    //p_pipe = pdcclient->sendmq;    // multi model use private pipe
    if(MULTIPIPE){ // multi model use private pipe to send ,
        p_pipe = pdcclient->sendmq;
        r = p_pipe->openpipe();   //WAIT for remote open, here is blocked
        if(r < 0) cerr<<"multipipe open client send failed"<<endl;
    }else{
        /*
        p_pipe = pdcclient->ackmq;
        r = p_pipe->openpipe();   //WAIT for remote open, here is blocked
        if(r < 0) cerr<<"simple pipe open client recv failed"<<endl;
        */
    }
    if(MULTIPIPE){
        Msginfo *msg = new Msginfo();
        msg->opcode = PDC_ADD_EPOLL;
    
        strcpy(msg->u.mgr.client.cluster,"ceph");
        strcpy(msg->u.mgr.client.pool, prados->GetName());
        strcpy(msg->u.mgr.client.volume,rbd_name);
        p_pipe = &pdcclient->msgmq;   // use public to send register info
        r = p_pipe->push(msg);
        delete msg;
    }
    }
    //sleep(1);
    cerr<<"pdc create rbd over"<<endl;
    return 0;

}


extern "C" int rbd_close(rbd_image_t image)
//int pdc_rbd_close(pdc_rbd_image_t image)
{
    
    
    return 0;
}
extern "C" int rbd_snap_create(rbd_image_t image, const char *snapname)
{
    printf("rbd_snap_create todo:\n");
    return 0;
}
extern "C" int rbd_snap_remove(rbd_image_t image, const char *snapname)
{

    printf("rbd_snap_remove \n");
    return 0;
}
extern "C" int rbd_snap_rollback(rbd_image_t image, const char *snapname)
{
    printf("rbd_snap_rollback");
    return 0;;
}
extern "C" int rbd_snap_list(rbd_image_t image, rbd_snap_info_t *snaps,
                               int *max_snaps)
{
    int r;
    printf("rbd_snap_list \n");
    if (*max_snaps < 1) {
        *max_snaps = 1;
        return -ERANGE;
    }  
    snaps[0].id = 0;
    snaps[0].size = 0;
    snaps[0].name = NULL;
    r = 0;
    return r;
}
extern "C" void rbd_snap_list_end(rbd_snap_info_t *snaps)
{
    printf("rbd_snap_list_end \n");
    return ;
}
extern "C" int rbd_aio_discard(rbd_image_t image, uint64_t off, uint64_t len,
			       rbd_completion_t c)
{
    printf("rbd_aio_discard \n");
    return 0;
}

extern "C" int rbd_aio_create_completion(void *cb_arg,
					 rbd_callback_t cb,
					 rbd_completion_t *c)
//int pdc_create_aio_complation(void *cb_arg, pdc_callback_t  cb,pdc_rbd_completion_t *c)
{
    PdcCompletion *comp = new PdcCompletion(cb, cb_arg);
    *c = (rbd_completion_t )comp;
    return 0;;
}

/*PdcCompletion *get_aio_completion(PdcAioCompletion* comp) {
  return this;
}*/

extern "C" int rbd_aio_write(rbd_image_t image, u64 off, size_t len,
			     const char *buf, rbd_completion_t c)

//int pdc_rbd_aio_write(pdc_rbd_image_t image, u64 off, size_t len,
                         //const char *buf ,pdc_rbd_completion_t c)
{
    int r;
    PdcCompletion *comp = (PdcCompletion *)c;
    BackendClient::RbdVolume *prbd = (BackendClient::RbdVolume *)image;
    PdcClient * pdc = pdc_client_mgr;
    r = pdc->aio_write(prbd,off, len, buf, comp);
    //r = prbd->aio_write(off, len, buf, comp);
    
    return r;
}
extern "C" int rbd_aio_read(rbd_image_t image, u64 off, size_t len,
			    char *buf, rbd_completion_t c)
{

    //cerr<<"rbd aio read is not imple"<<endl;
    int r;
    PdcCompletion *comp = (PdcCompletion *)c;
    BackendClient::RbdVolume*prbd = (BackendClient::RbdVolume*)image;
    PdcClient * pdc = pdc_client_mgr;
    
    r = pdc->aio_read(prbd, off,  len, buf, comp);
    

    return  r;

}

extern "C" int rbd_aio_flush(rbd_image_t image, rbd_completion_t c)
{
    

    printf("rbd_aio_flush \n");
    return 0;
}
extern "C" void rbd_aio_release(rbd_completion_t c)
//void pdc_aio_release(pdc_rbd_completion_t c)
{
    PdcCompletion *comp = (PdcCompletion *)c;
    comp->release();
}

/*
extern "C" void demo_completion(pdc_rbd_completion_t c,void *arg)
{
    PdcCompletion *comp = (PdcCompletion*)c;
    struct timeval endtime;
    cerr<<" IO finished:"<<*(int*)arg<<endl;
    
    ::gettimeofday(&endtime, NULL);
    
    cerr<<"end time is:"<<endtime.tv_sec<<"s + "<<endtime.tv_usec<<" us"<<endl;
}

*/
extern "C" int rbd_stat(rbd_image_t image, rbd_image_info_t *info,
	                                size_t infosize)
{
    info->size = 10737418240;//1024*1024*1024*10;
    info->obj_size = 1024*1024*4;
    info->num_objs = 256;
    info->order = 22;
    strcpy(info->block_name_prefix, "rbd_data.");

    return 0;
}
extern "C" int rbd_aio_wait_for_complete(rbd_completion_t c)
//extern "C" int pdc_rbd_aio_wait_for_complete(pdc_rbd_completion_t c)
{
    PdcCompletion *comp = (PdcCompletion *)c;

    comp->wait_for_complete();
    return 0;
}

/*
int main()
{
    int r = 0;
    int id= 0;
    pdc_rados_t prados;
    pdc_rados_ioctx_t ioctx;
    pdc_rbd_image_t img;
    struct timeval starttime;
    string vol("qemu-1");


//====================
    cerr<<"start test"<<endl;
    if(pdc_create_rados(&prados) < 0) {
        cerr<<"create prados failed "<<endl;
        return -1;
    }
    if(pdc_rados_conf_read_file(prados, NULL) < 0) {
        cerr<<"prados read conf file  failed "<<endl;
        return -1;
    }	
    if(pdc_connect_rados(prados) < 0) {
        cerr<<"create prados failed "<<endl;
        return -1;
    }	    
    if(pdc_rados_ioctx_create(prados,"wptest", &ioctx) < 0) {
        cerr<<"create prados failed "<<endl;
        return -1;
    }	       
    if(pdc_rbd_open(ioctx, &img , vol.c_str()) < 0) {
        cerr<<"create prados failed "<<endl;
        return -1;
    }	   

    int retry =5;
    int idx[retry+1];
    char *buf = (char *)malloc(1024);
    
    pdc_rbd_completion_t c[retry+1];
    while(retry){
    idx[retry]=retry;
    ::gettimeofday(&starttime, NULL);
    cerr<<"start time is:"<<starttime.tv_sec<<"s + "<<starttime.tv_usec<<" us"<<endl;;
    r = pdc_create_aio_complation((void *)&(idx[retry]),demo_completion, & c[retry]);
    
    r = pdc_rbd_aio_write(img, retry*1048576, 1024, buf,c[retry]);
    if(r< 0){
        cerr<<"rbd write failed"<<endl;
        //free(buf);
        return -1;
    }
    retry--;
    }
    //sleep(2);
    cerr<<"wait for 5 start:"<<endl;
    pdc_rbd_aio_wait_for_complete( c[5]);
    cerr<<"wait for 5 end:"<<endl;
    retry = 5;
    while(retry){
      pdc_aio_release(c[retry]);
      retry--;
    }
    sleep(1000);
    return 0;
}
*/

