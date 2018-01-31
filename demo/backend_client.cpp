/*
    
*/
#include "type.h"
#include "backend_client.hpp"
#include "pipe.hpp"
#include "pdcclient.hpp"


BackendClient::RadosClient::RadosClient(string nm, string _conf, BackendClient *_ceph):
    radosname(nm),confpath(_conf),ceph(_ceph)
{
    
}
	
int BackendClient::RadosClient::init(int create = 0)
{
    int r;
    cerr<<"start to connect rados:"<<radosname<<endl;
    if(create == 1)
    {}
    return 0;
}


int BackendClient::RbdVolume::init(int create = 0)
{
    int r;
    cerr<<"init rbd: "<<rbdname<<" pipemq"<<endl;
    
    //mq.insert(pari<string ,void *>("mq", sendq));
    //cerr<<"start to connect rbd:"<<rbdname<<endl;
    
    if(create == 1){
 
    }
    return 0;
}

int BackendClient::RbdVolume::aio_write(u64 offset, size_t len,const char *buf,
								PdcCompletion *c)
{
    BackendClient::RbdVolume*prbd = (BackendClient::RbdVolume*)this;
    //PdcCompletion *comp = (PdcCompletion*)c;
	
    //PdcClient *pdc = pdc_client_mgr;
    
    //PdcOp *op= new PdcOp();
    //Msginfo *msg = prbd->mq[RECVMQ].pop();
   // Msginfo *msg = new Msginfo();
	
	Msginfo *msg  = prbd->rados->ceph->msg_pool->malloc();
	msg->default_init();
    //msg->getopid();
    msg->opcode = PDC_AIO_WRITE;
    strcpy(msg->client.pool, prbd->rados->GetName());
    strcpy(msg->client.volume, prbd->rbdname.c_str());
    msg->originbuf = buf;
    msg->data.offset = offset;
    msg->data.len = len;
    msg->data.c = (void *)c;
    //op->volume = (void *)prbd;
    msg->insert_volume((void *)prbd);
    msg->dump("rbd aio write");

    /*add request to completion*/
    c->add_request();
    pthread_mutex_lock(prbd->rados->ceph->_mutex);
    prbd->rados->ceph->_queue->push_back(msg);
    pthread_mutex_unlock(prbd->rados->ceph->_mutex);
    return 0;
}




void* BackendClient::findclient(map<string, string> *opclient)
{
    void * vol;
    map<string ,string>::iterator it = opclient->begin();
    map<string, RadosClient*>::iterator itm = radoses.find(it->first);

    if(itm  != radoses.end()){
        RadosClient *r_volumes = itm->second;
        if(r_volumes->volumes.find(it->second)  != r_volumes->volumes.end()){
            vol = r_volumes->volumes[it->second];
            return (void*)vol;
        }
    }
    return NULL;
}
BackendClient::BackendClient(string nm,string _conf, list<Msginfo *>*msgop,
			pthread_mutex_t *mutex, MemPool<Msginfo> *_msg_pool):
    name(nm),_confpath(_conf),_queue(msgop),_mutex(mutex), msg_pool(_msg_pool)
{

}

