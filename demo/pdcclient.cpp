#include "rbd/librbd.h"
#include "rados/librados.h"


//#include "type.h"
#include "pdcclient.hpp"
//#include "backend_ceph.hpp"

int PdcClient::Iothreads::do_op(void * data)
{
    if( data)
        memset(data , 3, 4096);
    return 0;
}


void* PdcClient::Iothreads::_process()
{
    int r = 0;
    PdcClient *pdc = (PdcClient *)pc;
    CephBackend::RbdVolume *prbd;
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
    cerr<<"IOthread "<<pthread_self()<<" start"<<endl;
    
    while(1){
        if(stop()) continue;
        
        pthread_mutex_lock(&pdc->iomutex);
        if(pdc->ops.empty()){
            pthread_mutex_unlock(&pdc->iomutex);
            continue;
        }
        Msginfo *msg = pdc->ops.front();

        pdc->ops.pop_front();
        pthread_mutex_unlock(&pdc->iomutex);
        prbd = reinterpret_cast<CephBackend::RbdVolume *>(msg->pop_volume());
        if(!prbd){
            cerr<<"get NULL volume"<<endl;
            //delete op;
            assert(0);
            continue;
        }
        msg->dump("client io tp op");
        if(msg->opcode == PDC_AIO_WRITE){
            cerr<<"push op aio write:"<<msg->opid<<endl;
            p_pipe = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(prbd->mq[SENDMQ]);
            r = p_pipe->push(msg);
            if(r< 0){
                cerr<<"push op aio write:"<<msg->opid<<" failed"<<endl;
                delete msg;
                continue;
            }
        }
        
        delete msg;
    }

    return 0;
}


void* PdcClient::Finisherthreads::_process()
{
    PdcClient *pdc = (PdcClient *)pc;

    cerr<<"Fnisher thread "<<pthread_self()<<" start"<<endl;
    
    while(1){
        if(stop()) continue;
        
        pthread_mutex_lock(&pdc->finimutex);
        if(pdc->finishop.empty()){
            pthread_mutex_unlock(&pdc->finimutex);
            continue;
        }
        Msginfo *msg = pdc->finishop.front();
        pdc->finishop.pop_front();
        pthread_mutex_unlock(&pdc->finimutex);

        if(msg){
            msg->dump("client finish tp op");
            if(msg->opcode == RW_W_FINISH){
                cerr<<"write op:["<<msg->opid<<"] return:"<<msg->getreturnvalue()<<endl;
                PdcCompletion *c = reinterpret_cast<PdcCompletion*>(msg->data.c);
                if(c && c->callback){
                    c->callback(c->comp, c->callback_arg);
                
                }
                delete msg;
            }
           

	 }
        //free shared memory
        
        //TODO: here ,we connect rados  and write rbd
        
        
    }

return NULL;

}


void* PdcClient::Msgthreads::_process()
{
    int r;
    map<string , u64> sum;
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
    CephBackend::RbdVolume *prbd;
    PdcClient *pdc = (PdcClient *)pc;
    cerr<<"IOthread "<<pthread_self()<<" start"<<endl;
    
    while(1){
        if(stop() ) continue;
/*
        Msginfo *m = pdc->msgmq.pop();
        assert(m);
        Msginfo* msg = new Msginfo();
        msg = m;
        pdc->msgmq.clear();
*/
        pthread_mutex_lock(&pdc->msgmutex);
        if(pdc->msgop.empty()){
            pthread_mutex_unlock(&pdc->msgmutex);
            continue;
        }

        Msginfo *msg =  pdc->msgop.front();
        pdc->msgop.pop_front();
        pthread_mutex_unlock(&pdc->msgmutex);
        if(!msg){
            cerr<<"msg thread get NULL msg"<<endl;;
            assert(0);
            continue;
        }
        //performace->perf
        r = 0;
        if(msg){
            msg->dump("client msg tp op");
            std::map<string,string> client;
            assert(msg->client.cluster == "ceph");
            client[msg->client.pool] = msg->client.volume;
            //perf.inc();
            prbd = reinterpret_cast<CephBackend::RbdVolume *>(msg->volume);            if(msg->opcode == OPEN_RBD){
            //r = pdc->register_vm(client, msg);
            if(r < 0){
                cerr<<"register vm failed ret = "<<r <<endl;
                continue;
            }
            if(msg->opcode == OPEN_RADOS){
                cerr<<"do not come here "<<msg->opid<<endl;
		   continue;
            }else if(msg->opcode == OPEN_RBD){
                 cerr<<"do not come here "<<msg->opid<<endl;
                 continue;
            }
            else if(msg->opcode == PDC_AIO_WRITE){
            assert(prbd);
                msg->opcode =GET_MEMORY;
                list<u64> listadd ;
                //TODO:SET START TIME
                cerr<<"start to get memory"<<endl;
                p_pipe = (pdcPipe::PdcPipe<Msginfo>*)prbd->mq[SENDMQ];
                r = p_pipe->push(msg);
                if(r< 0){
                    cerr<<"get memory failed"<<endl;
                    msg->dump();
			delete msg;
                    continue;

                }
                //send msg and wait for ack:
                p_pipe = (pdcPipe::PdcPipe<Msginfo>*)prbd->mq[RECVMQ];
                Msginfo* m = p_pipe->pop();
                msg = m;
                p_pipe->clear();
                msg->opcode = PDC_AIO_WRITE;
                msg->dump();
                assert(msg->opcode == ACK_MEMORY);
                 pthread_mutex_lock(&pdc->iomutex);
                 //PdcOp *op = msg->op;
                 //assert(op);
                 pdc->ops.push_back(msg);
                 pthread_mutex_unlock(&pdc->iomutex);
                 cerr<<"op "<<msg->opid<<" to ops queue"<<endl;
                //TODO:SET END TIME
            }else if(msg->opcode == PDC_AIO_READ){
                /*
                PdcOp *op = new PdcOp(); 
                op->data.len = msg->data.len;
                op->data.indexlist.swap(msg->data.indexlist);
                op->client = msg->client;
                op->pid = msg->pid; // client pid;
                */
                pdc->OpFindClient(msg);
                pthread_mutex_lock(&pdc->msgmutex);
                pc->ops.push_back(msg);
                pthread_mutex_unlock(&pdc->msgmutex);

            }else if(msg->opcode == RW_W_FINISH || msg->opcode == RW_R_FINISH){
                //PdcOp *op = reinterpret_cast<PdcOp *>(msg->pop_op());
                //assert(op);
                pthread_mutex_lock(&pdc->finimutex);
                pc->finishop.push_back(msg);
                pthread_mutex_unlock(&pdc->finimutex);

            }


        }
        if(r < 0){
            cerr<<"msg do_op failed :"<<r <<endl;
            assert(0);
        }
        
    }

return NULL;

}

}

int PdcClient::init()
{
    int ret ;
    int r;
    int i;
    pthread_t id;
    string state("init: ");
    cerr<<state<<"start thread:"<<endl;

    CephBackend* pceph = new CephBackend("ceph","/etc/ceph/ceph.conf");
    clusters["ceph"] = pceph;
    //slab = new wp::shmMem::shmMem(MEMKEY, SERVERCREATE);
    ret = slab.Init();
    if(ret < 0){
        cerr<<"slab init faled:"<<ret<<endl;
    }
    map<string, void *>mqs;
    r = pdcPipe::createclientqueues(mqs, true);  //false means to create send pipe
    if(r< 0){
        cerr<<"pdc client init recvmq failed"<<endl;
        return r;
    }
    
    ackmq = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>* >( mqs[RECVMQ]);

    //msgmq = new wp::Pipe::Pipe(PIPEKEY, MEMQSEM, PIPEREAD,wp::Pipe::SYS_t::PIPESERVER);
    ret = msgmq.Init();
    if(ret < 0){
        cerr<<"msgmq init faled:"<<ret<<endl;
    }
    
    pthread_mutex_init(&iomutex,NULL);
    iothread = new PdcClient::Iothreads("IO-threadpool", this);
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


void PdcClient::OpFindClient(Msginfo *&op)
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



