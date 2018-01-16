//g++ msserver.c -o server
//g++ msclient.c -o client
#include "type.h"
#include <iostream>
#include <sstream>

#include "pdcclient.hpp"

PdcClient *pdc_client_mgr;


int pdc_create_rados(pdc_rados_t *prados)
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
    }
    vmclient->inc_ref();
    *prados = vmclient;
    return 0;
}

int pdc_rados_conf_read_file(pdc_rados_t vmclient, const char * path)
{
    int r =0;
    //todo:
    // to tell server which file to read;
    
    return r ;
}

int pdc_rados_conf_set(pdc_rados_t cluster, const char *option, const char *value)
{
    cerr<<"pdc_rados_conf_set is null now"<<endl;
    return 0;
}

int pdc_connect_rados(pdc_rados_t vmclient)
{
    PdcClient * pclient = reinterpret_cast<PdcClient *>(vmclient);
    cerr<<"pdc_connect_rados "<<endl;
    return 0;
}

int pdc_rados_ioctx_create(pdc_rados_t vmclient, const char *pool_name,
                                  pdc_rados_ioctx_t *ioctx)
{
    int r;
    CephBackend::RadosClient *prados;
    string radosname(pool_name);
    PdcClient * pclient = reinterpret_cast<PdcClient *>(vmclient);
    
    CephBackend *pceph = pclient->clusters["ceph"];

    map<string ,CephBackend::RadosClient *>::iterator it = pceph->radoses.find(radosname);
    if(it == pceph->radoses.end()){  // not exist ,create a new one
        prados = new CephBackend::RadosClient(radosname, "/etc/ceph/ceph.conf",pceph);
        pceph->radoses[radosname] = prados;
            
        Msginfo *msg = new Msginfo();
        msg->opcode = OPEN_RADOS;
        strcpy(msg->client.cluster,"ceph");
        strcpy(msg->client.pool,pool_name);
    

        strcpy(msg->mqkeys.key , pclient->ackmq->Getkeys());
        msg->mqkeys.semkey= pclient->ackmq->GetSemKey();
        msg->dump("open rados");
        r = pclient->msgmq.push(msg);
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

void pdc_rados_ioctx_destroy(pdc_rados_ioctx_t io)
{
    cerr<<"pdc_rados_ioctx_destroy is null now"<<endl;

    
}



void pdc_rados_shutdown(pdc_rados_t cluster)
{
    cerr<<"pdc_rados_shutdown is null now"<<endl;
    return ;
}
ssize_t pdc_rbd_aio_get_return_value(pdc_rbd_completion_t c)
{
    PdcCompletion *comp = (PdcCompletion*)c;
    return comp->retcode; 
}

int pdc_rbd_open(pdc_rados_ioctx_t ioctx,pdc_rbd_image_t * image,const char * rbd_name)
{
    int r;
    string pkey;
    int skey = 0;
    string rbdname(rbd_name);
    string poolname;
    PdcClient *pdcclient = pdc_client_mgr;

    CephBackend::RbdVolume*prbd;
    //PdcClient * pclient = pdc_client_mgr;
    CephBackend::RadosClient *prados = reinterpret_cast<CephBackend::RadosClient *>(ioctx);

    if(prados->volumes.find(rbdname) != prados->volumes.end()){  //exist
        cerr<<"find pdb exist"<<endl;
        prbd = reinterpret_cast<CephBackend::RbdVolume*>(prados->volumes[rbdname]);
        
    }else {    //NOT EXIST
        prbd = new CephBackend::RbdVolume(rbdname, prados);
        
        pdcPipe::copymqs(prbd->mq, &pdcclient->msgmq,pdcclient->ackmq);     
        
        r = prbd->init(0);
        if(r< 0){
            cerr<<"client create new rbd:"<<rbdname<<" failed :"<<r<<endl;
            return -1;
        }
        *image = (void *)prbd;
        prados->volumes[rbdname] = (void *)prbd;
		
    pdcPipe::PdcPipe<Msginfo>::ptr recvmq = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(prbd->mq[RECVMQ]);
    //msginfo will change to msgpool list  next step
    Msginfo *msg = new Msginfo();
    msg->opcode = OPEN_RBD;
    
    strcpy(msg->client.cluster,"ceph");
    strcpy(msg->client.pool, prados->GetName());
    strcpy(msg->client.volume,rbd_name);
    strcpy(msg->mqkeys.key,recvmq->Getkeys());
    msg->mqkeys.semkey = recvmq->GetSemKey();

    msg->dump("open rbd");
    
    r = pdcclient->msgmq.push(msg);
    delete msg;
    if(r<  0 ){
        cerr<<" create remote rados failed"<<endl;
        
        return -1;
    }
    
    }

    cerr<<"pdc create rbd over"<<endl;
    return 0;

}


int pdc_rbd_close(pdc_rbd_image_t image)
{
    
    
    return 0;
}

int pdc_create_aio_complation(void *cb_arg, pdc_callback_t  cb,pdc_rbd_completion_t *c)
{
    PdcCompletion *comp = new PdcCompletion(cb, cb_arg, (void*)c);
    *c = (void *)comp;
    return 0;;
}

int pdc_rbd_aio_write(pdc_rbd_image_t image, u64 off, size_t len,
                         const char *buf ,pdc_rbd_completion_t c)
{
    int r;
    CephBackend::RbdVolume*prbd = (CephBackend::RbdVolume*)image;

    r = prbd->aio_write(off,  len, buf, c);
    
    return 0;
}

void pdc_aio_release(pdc_rbd_completion_t c)
{
    PdcCompletion *comp = (PdcCompletion*)c;
    comp->release();

}
void demo_completion(pdc_rbd_completion_t c,void *arg)
{
    PdcCompletion *comp = (PdcCompletion*)c;
    struct timeval endtime;
    cerr<<" IO finished:"<<*(int*)arg<<endl;
    
    ::gettimeofday(&endtime, NULL);
    
    cerr<<"end time is:"<<endtime.tv_sec<<"s + "<<endtime.tv_usec<<" us"<<endl;
}

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
    sleep(2);
    retry = 5;
    while(retry){
      pdc_aio_release(c[retry]);
      retry--;
    }
    sleep(1000);
    return 0;
}

