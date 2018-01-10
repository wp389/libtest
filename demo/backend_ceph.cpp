/*
    
*/
#include "type.h"
#include "backend_ceph.hpp"
#include "pipe.hpp"



PdcCompletion::PdcCompletion(pdc_callback_t cb, void *cb_arg, void *c):
    comp(c)
{
    callback = cb;
    callback_arg = cb_arg;
}

PdcCompletion::PdcCompletion(void *c):
    comp(c)
{
    
}

int PdcCompletion::complate()
{

    cerr<<"pdc complate op:"<<opidx<<endl;
    if(callback)
        callback(comp,callback_arg);

    
    return 0;
}

CephBackend::RadosClient::RadosClient(string nm, string _conf):
    radosname(nm),confpath(_conf),cluster(NULL)
{
    
}
	
int CephBackend::RadosClient::init()
{
    int r;
    cerr<<"start to connect rados:"<<radosname<<endl;
	/*

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
	*/

    return 0;
}


int CephBackend::RbdVolume::init()
{
    int r;
    cerr<<"init rbd: "<<rbdname<<" pipemq"<<endl;
    
    //mq.insert(pari<string ,void *>("mq", sendq));
    cerr<<"start to connect rbd:"<<rbdname<<endl;
    
/*
    r = rbd_open(rados->ioctx, rbdname.c_str(), &image , NULL);
    if(r< 0){
        cerr<<"open rbd volume failed:"<<rbdname<<" r="<<r<<endl;
        return r;
    }  
*/
    return 0;
}
int CephBackend::RbdVolume::aio_write(u64 offset, size_t len,const char *buf, pdc_rbd_completion_t c)
{
    CephBackend::RbdVolume*prbd = (CephBackend::RbdVolume*)this;
    PdcCompletion *comp = (PdcCompletion*)c;
	
    //PdcClient *pdc = pdc_client_mgr;
    
    //PdcOp *op= new PdcOp();
    //Msginfo *msg = prbd->mq[RECVMQ].pop();
    Msginfo *msg = new Msginfo();
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

    /*
    pthread_mutex_lock(&pdc->iomutex);
    pdc->msgop.push_back(msg);
    pthread_mutex_unlock(&pdc->iomutex);
    */
    return 0;
}

int CephBackend::register_client(map<string,string > &vmclient, Msginfo *msg)
{
    int r;
    CephBackend *cephcluster ;
    string nm("ceph");
	
    if(vmclient.empty()) {
        cerr<<"register NULL client"<<endl;
        assert(0);
        return -1;
    }

    map<string ,string>::iterator it = vmclient.begin();
    map<string, RadosClient*>::iterator itm = radoses.find(it->first);
    if(itm  != radoses.end()){
        cerr<<"rados pool:"<<it->first<<" had existed"<<endl;
        if(vols.find(it->second)  != vols.end()){
            cerr<<"rbd "<< it->second<<" had existed"<<endl;
            //update matedata
            
        }else{
            cerr<<"rbd "<< it->second<<" register now "<<endl;
            CephBackend::RbdVolume * rbd = new CephBackend::RbdVolume(it->second,itm->second);
            if(rbd->init() < 0) return -1;
            //vols[it->second] = rbd;
            vols.insert(pair<string ,RbdVolume*>(it->second ,rbd));
        }
    }else{
        CephBackend::RadosClient *rados = new CephBackend::RadosClient(it->first, "/etc/ceph/ceph.conf");
        if(rados->init() < 0){
            return -1;
        }
        radoses[it->first] = rados;
        CephBackend::RbdVolume * rbd = new CephBackend::RbdVolume(it->second,rados);
        if(rbd->init() < 0) return -1;
        
        r = pdcPipe::createserverqueues(msg->pipekeys,msg->semkeys,rbd->mq);
        if(r < 0){
            cerr<<"create server queues failed"<<endl;
            return r;
        }
        vols[it->second] = rbd;
    }

	
return 0;
}

void* CephBackend::findclient(map<string, string> *opclient)
{
    RbdVolume * vol;
    map<string ,string>::iterator it = opclient->begin();
    map<string, RadosClient*>::iterator itm = radoses.find(it->first);

    if(itm  != radoses.end()){
        if(vols.find(it->second)  != vols.end()){
            vol = vols[it->second];
            return (void*)vol;
        }
    }
    return NULL;
}
CephBackend::CephBackend(string nm,string _conf):
    name(nm),_confpath(_conf)
{

}

