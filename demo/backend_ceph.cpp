/*
    
*/
#include "type.h"
#include "backend_ceph.hpp"
#include "pipe.hpp"
#include "shmmem.hpp"
CephBackend::RadosClient::RadosClient(string nm, string _conf, CephBackend *_ceph):
    radosname(nm),confpath(_conf),ceph(_ceph),cluster(NULL)
{
    
}
	
int CephBackend::RadosClient::init(int create = 0)
{
    int r;
    cerr<<"start to connect rados:"<<radosname<<endl;
  if(create == 1){
  if(!SERVER_IO_BLACKHOLE){
    r= rados_create(&cluster ,NULL);
    if(r< 0){
        cerr<<"create rados failed:"<<radosname<<" r="<<r<<endl;
        return r;
    }
    r = rados_conf_read_file(cluster, NULL);
    if(r< 0){
        cerr<<"read rados conf  failed:"<<radosname<<" r="<<r<<endl;
        return r;
    }
    r = rados_connect(cluster);
    if(r< 0){
        cerr<<"connect rados failed:"<<radosname<<" r="<<r<<endl;
        return r;
    }
    r = rados_ioctx_create(cluster, radosname.c_str(), &ioctx);
    if(r< 0){
        cerr<<"create rados ioctx failed:"<<radosname<<" r="<<r<<endl;
        return r;
    }  
    }
  }
    return 0;
}


int CephBackend::RbdVolume::init(int create = 0)
{
    int r;
    cerr<<"init rbd: "<<rbdname<<" pipemq"<<endl;
    
    //mq.insert(pari<string ,void *>("mq", sendq));
    //cerr<<"start to connect rbd:"<<rbdname<<endl;
    
    if(create == 1){
        if(!SERVER_IO_BLACKHOLE){
        r = rbd_open(rados->ioctx, rbdname.c_str(), &image , NULL);
        if(r< 0){
            cerr<<"open rbd volume failed:"<<rbdname<<" r="<<r<<endl;
            return r;
        }  
        }
    }
    return 0;
}
/*
int CephBackend::RbdVolume::aio_write(u64 offset, size_t len,const char *buf, pdc_rbd_completion_t c)
{
    CephBackend::RbdVolume*prbd = (CephBackend::RbdVolume*)this;
    PdcCompletion *comp = (PdcCompletion*)c;
	
    //PdcClient *pdc = pdc_client_mgr;
    
    //PdcOp *op= new PdcOp();
    //Msginfo *msg = prbd->mq[RECVMQ].pop();
    Msginfo *msg = new Msginfo();
    //msg->getopid();
    msg->opcode = PDC_AIO_WRITE;
    strcpy(msg->client.pool, prbd->rados->GetName());
    strcpy(msg->client.volume, prbd->rbdname.c_str());
    msg->originbuf = buf;
    msg->data.offset = offset;
    msg->data.len = len;
    msg->data.c = c;
    //op->volume = (void *)prbd;
    msg->insert_volume((void *)prbd);
    msg->dump("rbd aio write");


    prbd->rados->ceph->_queue->push_back(msg);

    return 0;
}
*/


int CephBackend::RbdVolume::do_create_rbd_completion(void * op, rbd_completion_t *comp )
{
    int r;
    
    rbd_aio_create_completion(op,  pdc_callback, comp);

return 0;
}
int CephBackend::RbdVolume::do_aio_write(void *_op,u64 offset, size_t len,const char *buf, pdc_rbd_completion_t c)
{
    int r;
    Msginfo* op= (Msginfo *)_op;
    rbd_completion_t comp;
    if(!image)  return -1;
    op->ref_inc();
    //do_create_rbd_completion(op ,&comp);
    r = rbd_aio_write(image, offset, len, buf, c);

return r;
}
int CephBackend::RbdVolume::do_aio_read(void *_op,u64 offset, size_t len, char *buf, pdc_rbd_completion_t c)
{
    int r;
    Msginfo* op= (Msginfo *)_op;
    rbd_completion_t comp;
    if(!image)  return -1;
    op->ref_inc();
    //do_create_rbd_completion(op ,&comp);
    r = rbd_aio_read(image, offset, len, buf, c);

return r;
}


int CephBackend::register_client(map<string,string > &vmclient, Msginfo *msg)
{
    int r;
    CephBackend *cephcluster ;
    string nm("ceph");
    RadosClient *p_rados;
        

    if(vmclient.empty()) {
        cerr<<"register NULL client"<<endl;
        assert(0);
        return -1;
    }
    //msg->dump("register_client");
    //cerr<<"vmclient:"<<vmclient.size()<<" radoses:"<<radoses.size()<<endl;
    map<string ,string>::iterator it = vmclient.begin();
    map<string, RadosClient*>::iterator itm = radoses.find(it->first);
    if(itm  != radoses.end()){
        p_rados = itm->second;
        cerr<<"rados pool:"<<it->first<<" had existed"<<endl;
        if(p_rados->volumes.find(it->second)  != p_rados->volumes.end()){
            cerr<<"rbd "<< it->second<<" had existed and update pipe"<<endl;
            //update matedata or pipe
            //TODO:
            CephBackend::RbdVolume * rbd = reinterpret_cast<CephBackend::RbdVolume *>(p_rados->volumes[it->second]);
            r = pdcPipe::updateserverqueues((void *)&msg->mqkeys,rbd->mq);
            if(r < 0){
                cerr<<"update server queues failed"<<endl;
                return r;
            }

			
        }else{
            cerr<<"rbd "<< it->second<<" register now "<<endl;
            CephBackend::RbdVolume * rbd = new CephBackend::RbdVolume(it->second,itm->second);
            if(rbd->init(1) < 0) return -1;
            
            r = pdcPipe::createserverqueues((void *)&msg->mqkeys,rbd->mq);
            if(r < 0){
                cerr<<"create server queues failed"<<endl;
                return r;
            }
            vols.insert(pair<string ,RbdVolume*>(it->second ,rbd));
            p_rados->volumes[it->second] = (void *)rbd;
            
        }
    }else{
        CephBackend::RadosClient *rados = new CephBackend::RadosClient(it->first, "/etc/ceph/ceph.conf",this);
        if(rados->init(1) < 0){
            return -1;
        }
        radoses[it->first] = rados;
        CephBackend::RbdVolume * rbd = new CephBackend::RbdVolume(it->second,rados);
        if(rbd->init(1) < 0) return -1;
        
        r = pdcPipe::createserverqueues((void *)&msg->mqkeys,rbd->mq);
        if(r < 0){
            cerr<<"create server queues failed"<<endl;
            return r;
        }
        vols[it->second] = rbd;
        rados->volumes[it->second] = (void*) rbd;
    }

	
return 0;
}

void* CephBackend::findclient(map<string, string> *opclient)
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
CephBackend::CephBackend(string nm,string _conf, list<Msginfo *>*msgop):
    name(nm),_confpath(_conf),_queue(msgop)
{

}

