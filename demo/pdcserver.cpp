//#include "rbd-bak/librbd.h"
//#include "rados-bak/librados.h"


//#include "type.h"
#include "pdcserver.hpp"
//#include "backend_ceph.hpp"

Pdcserver*pdc_server_mgr;



void pdc_callback(rbd_completion_t cb, void *arg)
{
    int r;
    int n;
    Pdcserver * pdc = pdc_server_mgr;
    Msginfo *op = (Msginfo*)arg;
    //shmMem::ShmMem<simpledata> *shm = reinterpret_cast<shmMem::ShmMem<simpledata> *>(op->slab);
    op->dump("pdc_callback");
    //cerr<<"server get rbd callback"<<endl;
    op->ref_dec();
    if(cb)
        op->return_code |= rbd_aio_get_return_value(cb);
    else
        op->return_code = 0;
    if(op->isdone()){
        CephBackend::RbdVolume *prbd = (CephBackend::RbdVolume *)op->volume;
        
        if(! prbd) assert(0);
        if(op->opcode == PDC_AIO_WRITE) {
            op->opcode =  RW_W_FINISH;
            //todo put shmmemory keys .  
            
            if(pdc){
                vector<u64> index(op->data.indexlist,op->data.indexlist+sizeof(op->data.indexlist)/sizeof(u64));
                n = pdc->slab.put(index);
                if(r < 0 ){
                    cerr<<"shm->put falied:"<<r<<endl;
                    //return ;
                    
                }
            }
            
        }
        pdcPipe::PdcPipe<Msginfo>*p_pipe = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(prbd->mq[SENDMQ]);
        r = p_pipe->push(op);
    }
    if(cb)
        rbd_aio_release(cb);
    
    //cerr<<"pdc_callback , now ref is:"<<op->ref << "  return_code ="<<op->return_code <<" put mem:"<<n<<endl;;
    
}

int Pdcserver::Iothreads::do_op(void * data)
{
    if( data)
        memset(data , 3, 4096);
    return 0;
}
Msginfo* Pdcserver::register_put(Msginfo *op)
{
    //Putop *p_op = new Putop(op, &slab);
    op->slab = &slab;
    return op;
}

void* Pdcserver::Iothreads::_process()
{
    int r = 0;
    u64 sum =0;
    static bool flag = false;
    Pdcserver *pdc = (Pdcserver *)server;
    CephBackend::RbdVolume *vol;
    cerr<<"IOthread "<<pthread_self()<<" start"<<endl;
    
    while(1){
        if(stop()) continue;
        
        pthread_mutex_lock(&pdc->iomutex);
        if(pdc->ops.empty()){
            pthread_mutex_unlock(&pdc->iomutex);
            continue;
        }
        Msginfo *op = pdc->ops.front();
        pdc->ops.pop_front();
        pthread_mutex_unlock(&pdc->iomutex);

        op->dump("server io tp op");
        pdc->OpFindClient(op);
        vol = reinterpret_cast<CephBackend::RbdVolume *>(op->volume);
        if(!vol){
            op->dump("get NULL volume");
            cerr<<"get NULL volume"<<endl; 
            continue;
        }
        sum++;
        if(SERVER_IO_BLACKHOLE){    //black hole
        rbd_completion_t comp;
        u64 off = op->data.offset;
        u32 bufsize = op->data.len;
        u32 lengh;

        flag = false;
        vol->do_create_rbd_completion(op, &comp);
        for(int i = 0;i < op->data.chunksize;i++){
            //memset(op->data.pdata, 6, op->data.len);
            simpledata * pdata = pdc->slab.getaddbyindex(op->data.indexlist[i]);
            //TODO: WRITE
            lengh = bufsize > sizeof(simpledata) ? sizeof(simpledata):bufsize;
            vol->do_aio_write(op, off+ i*sizeof(simpledata), lengh, (char *)pdata, comp);   
        }
        //cerr<<"do rbd write---------:"<<sum<<endl;
        }else{
            if(!flag){
                flag = true;
                cerr<<"********server start to use black hole*******"<<endl;
            }
            op->ref_inc();
            pdc_callback(NULL, op);

        }
    }

    return 0;
}


void* Pdcserver::Finisherthreads::_process()
{
    int r;
    u64 sum = 0;
    Pdcserver *pdc = (Pdcserver *)server;
    CephBackend::RbdVolume *prbd;
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
    cerr<<"listen thread "<<pthread_self()<<" start"<<endl;
    
    while(1){
        if(stop()) continue;
        Msginfo *msg = pdc->msgmq.pop();
        if(!msg){
            continue;
        }
        Msginfo *op = new Msginfo();
        op->copy(msg);
        pdc->msgmq.clear();

        if(op){
        op->dump("listen thread get op");
        if(op->opcode == PDC_AIO_WRITE){
            //op->opcode == PDC_AIO_WRITE;
            pthread_mutex_lock(&pdc->iomutex);
            pdc->ops.push_back(op);
            pthread_mutex_unlock(&pdc->iomutex);
        }
        else if(op->opcode == GET_MEMORY){
            pthread_mutex_lock(&pdc->msgmutex);
            pdc->msgop.push_back(op);
            pthread_mutex_unlock(&pdc->msgmutex);
        }else{
            pthread_mutex_lock(&pdc->msgmutex);
            pdc->msgop.push_back(op);
            pthread_mutex_unlock(&pdc->msgmutex);

        }
        sum++;
        //op->dump("server finish tp op");
        //cerr<<" get a finish op ,do pop"<<endl;
        }
    }

return 0;

}


void* Pdcserver::Msgthreads::_process()
{
    int r;
    u64 sum = 0;
    CephBackend::RbdVolume *vol;
    Pdcserver *pdc = (Pdcserver *)server;
    cerr<<"MSGthread "<<pthread_self()<<" start"<<endl;

    while(1){
        if(stop() ) continue;
        pthread_mutex_lock(&pdc->msgmutex);
        if(pdc->msgop.empty()){
            pthread_mutex_unlock(&pdc->msgmutex);
            continue;
        }
        Msginfo *msg = pdc->msgop.front();
        pdc->msgop.pop_front();
        pthread_mutex_unlock(&pdc->msgmutex);

/*
        Msginfo *msg = pdc->msgmq.pop();
        if(!msg){
            
            continue;
        }
*/
        if(msg){
            sum++;
            assert(msg);
            msg->dump("server msg tp op");
            std::map<string,string> client;
            //assert(msg->client.cluster == "ceph");
            client[msg->client.pool] = msg->client.volume;
            //perf.inc();
            switch(msg->opcode){
            //if(msg->opcode == OPEN_RADOS){
            case OPEN_RADOS:
            {
                pdc->register_connection(msg);
                break;
            }
            //}else if(msg->opcode == OPEN_RBD){
            case OPEN_RBD:
            {
                r = pdc->register_vm(client, msg);
                if(r < 0){
                    cerr<<"register vm failed ret = "<<r <<endl;
                    continue;
                }
                break;
            }
            //}else if(msg->opcode == GET_MEMORY){
            case GET_MEMORY:
            {
                //vector<u64> listadd ;
                pdc->OpFindClient(msg);
                r = pdc->slab.get(msg->data.len, msg->data.indexlist);
                if(r <= 0){
                    cerr<<"get memory failed"<<endl;
                    continue;
                }
                msg->data.chunksize = r;
                msg->opcode = ACK_MEMORY;
                //msg->data.indexlist.swap(listadd);
                pdcPipe::PdcPipe<Msginfo>::ptr pipe;

                CephBackend::RbdVolume *vol = reinterpret_cast<CephBackend::RbdVolume *>(msg->volume);
                pipe =reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(vol->mq[SENDMQ]);
                //pipe = server->ackmq[client];
                r = pipe->push(msg);
                if(r < 0){
                    msg->dump("push failed");
                    cerr<<"pipe push msg:"<<msg->opid<<" failed"<<endl;
                    continue;
                }
                break;
            }
            //}else if(msg->opcode == PDC_AIO_WRITE){
            default:
            {
                /*

                pdc->OpFindClient(msg);
                pthread_mutex_lock(&pdc->iomutex);
                server->ops.push_back(msg);
                pthread_mutex_unlock(&pdc->iomutex);
                */
                cerr<<"opcode error:"<<msg->opcode<<endl;
                assert(0);
                break;
            }

         }
        }

    }

return 0;

}


int Pdcserver::init()
{
    int ret ;
    int i;
    pthread_t id;
    string state("init: ");
    cerr<<state<<"start thread:"<<endl;
    
    CephBackend* pceph = new CephBackend("ceph","/etc/ceph/ceph.conf", &msgop);
    clusters["ceph"] = pceph;
    pceph->radoses.clear();
    pceph->vols.clear();
    //slab = new wp::shmMem::shmMem(MEMKEY, SERVERCREATE);
    ret = slab.Init();
    if(ret < 0){
        cerr<<"slab init faled:"<<ret<<endl;
        return -1;
    }

    //msgmq = new wp::Pipe::Pipe(PIPEKEY, MEMQSEM, PIPEREAD,wp::Pipe::SYS_t::PIPESERVER);
    ret = msgmq.Init();
    if(ret < 0){
        cerr<<"msgmq init faled:"<<ret<<endl;
    }
    pthread_mutex_init(&iomutex,NULL);
    iothread = new Pdcserver::Iothreads("IO-threadpool", this);
    iothread->init(1);

    pthread_mutex_init(&msgmutex,NULL);
    msgthread = new Msgthreads("MSG-threadpool",this);
    msgthread->init(2);

    pthread_mutex_init(&finimutex,NULL);
    finisher = new Finisherthreads("Finisher threadpool", this);
    finisher->init(1);	
    ops.clear();
    finishop.clear();
    msgop.clear();

    finisher->start();
    iothread->start();
    msgthread->start();
    
    cerr<<"pdcserver init over"<<endl;
    return 0;
}

int Pdcserver::register_connection(Msginfo* msg)
{
    //msg->dump();
    cerr<<"msg to register pipe connection"<<endl;

    return 0;

}

int Pdcserver::register_vm(map<string,string > &client, Msginfo *msg)
{
    CephBackend *cephcluster ;
    string nm("ceph");

    if(client.empty()) {
        cerr<<"register NULL client"<<endl;
        assert(0);
        return -1;
    }
    if(clusters.empty()){
        cerr<<" create new ceph backend "<<endl;
        cephcluster= new CephBackend(nm,"/etc/ceph/ceph.conf", &msgop);
        clusters[nm] = cephcluster;
    }else{
        cerr<<" find exist ceph backend "<<endl;
        cephcluster = clusters[nm];
    }
    assert(cephcluster);
    cerr<<"register vm:"<<client.size()<<endl;
    cephcluster->register_client(client,msg);

    /*
    map<string,string>::iterator it = client.begin();
    if(it != client.end()){
        cerr<<"register pipe:"<<it->first<<" "<<it->second<<endl;
        CephBackend::RadosClient *rados = clusters[nm]->radoses[it->first];
        CephBackend::RbdVolume *prbd = reinterpret_cast<CephBackend::RbdVolume *>(rados->volumes[it->second]);
        pdcPipe::PdcPipe<Msginfo>::ptr mq =  reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(prbd->mq[SENDMQ]);  
        //ackmq.insert(map<map<string,string>, pdcPipe::PdcPipe<Msginfo>*  >(pair(client,mq)));
        assert(mq);
        ackmq[client] = mq;

    }
    */
return 0;
}

void Pdcserver::OpFindClient(Msginfo *&op)
{
    string pool(op->client.pool);
    string volume(op->client.volume);
    map<string,string> opclient;
    opclient[pool] = volume;
    string backendname("ceph");
    CephBackend *backend = clusters[backendname];
    if(!backend){
        cerr<<"find no backend now for:"<<backendname<<endl;
        assert(0);
        return;
    }
    op->volume =  backend->findclient( &opclient);
    assert(op->volume);
    
}



int main()
{
    int r ;
    //Time time();
    cerr<<"pdc server start:"<<endl;
    Pdcserver *server = new Pdcserver("wp");
    r = server->init();
    pdc_server_mgr = server;
    if(r < 0){
        cerr<<"server init failed"<<endl;
        return -1;
    }
    r = 0;
    while(1){
       r++;
	sleep(1);
    }
    
    return 0;
}

