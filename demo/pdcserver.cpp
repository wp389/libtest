#include "rbd/librbd.h"
#include "rados/librados.h"


//#include "type.h"
#include "pdcserver.hpp"
//#include "backend_ceph.hpp"

int Pdcserver::Iothreads::do_op(void * data)
{
    if( data)
        memset(data , 3, 4096);
    return 0;
}

void* Pdcserver::Iothreads::_process()
{
    int r = 0;
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
        vol = reinterpret_cast<CephBackend::RbdVolume *>(op->volume);
        if(!vol){
            op->dump("get NULL volume");
            cerr<<"get NULL volume"<<endl; 
            continue;
        }
        pdc->ops.pop_front();
        pthread_mutex_unlock(&pdc->iomutex);
        op->dump("server io tp op");

        for(int i = 0;i < op->data.chunksize;i++){
            //memset(op->data.pdata, 6, op->data.len);
            simpledata * pdata = pdc->slab.getaddbyindex(op->data.indexlist[i]);
            //TODO: WRITE
            r = do_op(pdata);
        }
        //TODO: here ,we connect rados  and write rbd
        op->return_code = 0;
        if(op->opcode == PDC_AIO_WRITE) op->opcode =  RW_W_FINISH;
        cerr<<"do io:"<< vol->GetName() <<endl;
        pthread_mutex_lock(&pdc->finimutex);
        pdc->finishop.push_back(op);
        pthread_mutex_unlock(&pdc->finimutex);
       
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
    cerr<<"Fnisher thread "<<pthread_self()<<" start"<<endl;
    
    while(1){
        if(stop()) continue;
        
        pthread_mutex_lock(&pdc->finimutex);
        if(pdc->finishop.empty()){
            pthread_mutex_unlock(&pdc->finimutex);
            continue;
        }
        Msginfo *op = pdc->finishop.front();
        pdc->finishop.pop_front();
        pthread_mutex_unlock(&pdc->finimutex);
        op->dump("server finish tp op");
        //cerr<<" get a finish op ,do pop"<<endl;
        op->return_code = 0;
        //free shared memory
        prbd = reinterpret_cast<CephBackend::RbdVolume *>(op->volume);
        //TODO: here ,we connect rados  and write rbd
        op->return_code = 0;
        if(prbd)
            p_pipe = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>* >(prbd->mq[SENDMQ]);
        else
            assert(0);
        r = p_pipe->push(op);
        sum++;
        if(r < 0){
            cerr<<"done op id:"<<sum<<" failed"<<endl;
	     continue;
        }
		
    }

return 0;

}


void* Pdcserver::Msgthreads::_process()
{
    int r;
    Pdcserver *pdc = (Pdcserver *)server;
    cerr<<"MSGthread "<<pthread_self()<<" start"<<endl;

    while(1){
        if(stop() ) continue;
        Msginfo *msg = pdc->msgmq.pop();
        if(!msg){
            ///cerr<<"msg thread get NULL msg"<<endl;;
            //assert(0);
            continue;
        }
        //performace->perf
        r = 0;
        //r = do_op(msg);
        if(msg){
            assert(msg);
            msg->dump("server msg tp op");
            std::map<string,string> client;
            //assert(msg->client.cluster == "ceph");
            client[msg->client.pool] = msg->client.volume;
            //perf.inc();
            if(msg->opcode == OPEN_RADOS){
                pdc->register_connection(msg);


            }else if(msg->opcode == OPEN_RBD){
                r = pdc->register_vm(client, msg);
                if(r < 0){
                    cerr<<"register vm failed ret = "<<r <<endl;
                    continue;
                }
                
            }else if(msg->opcode == GET_MEMORY){
                //vector<u64> listadd ;
                pdc->OpFindClient(msg);
                r = pdc->slab.get(msg->data.len, msg->data.indexlist);
                if(r < 0){
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
                    msg.dump("push failed");
                    cerr<<"pipe push msg:"<<msg->opid<<" failed"<<endl;
                    continue;
                }
            }else if(msg->opcode == PDC_AIO_WRITE){
                /*
                PdcOp *op = new PdcOp(); 
                op->data.len = msg->data.len;
                op->data.indexlist.swap(msg->data.indexlist);
                op->client = msg->client;
                op->pid = msg->pid; // client pid;
                */
                pdc->OpFindClient(msg);
                pthread_mutex_lock(&pdc->msgmutex);
                server->ops.push_back(msg);
                pthread_mutex_unlock(&pdc->msgmutex);
                
            }


        }
        if(r < 0){
            cerr<<"msg do_op failed :"<<r <<endl;
            assert(0);
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
    msgthread->init(1);

    pthread_mutex_init(&finimutex,NULL);
    finisher = new Finisherthreads("Finisher threadpool", this);
    finisher->init(1);	
    //register_client();
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

