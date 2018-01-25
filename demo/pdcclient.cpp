#include "rbd/librbd.h"
#include "rados/librados.h"


//#include "type.h"
#include "pdcclient.hpp"
//#include "backend_ceph.hpp"

int PdcClient::Iothreads::do_op(void * data)
{

    

    return 0;
}


void* PdcClient::Iothreads::_process()
{
    int r = 0;
    PdcClient *pdc = (PdcClient *)pc;
    BackendClient::RbdVolume *prbd;
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
    cerr<<"IOthread "<<pthread_self()<<" start"<<endl;
    
    while(1){
        if(stop()) continue;
        
        pdc->iolock.lock();

        //pdc->opcond.wait( pdc->iolock);
        list<Msginfo*>oplist;
        oplist.swap(pdc->ops);
        //Msginfo *msg = pdc->ops.front();
        //pdc->ops.pop_front();
        pdc->iolock.unlock();

        while(!oplist.empty()){
        Msginfo *msg = oplist.front();
        oplist.pop_front();
        msg->dump("iothread: client io tp op");
        if(msg->opcode == PDC_AIO_WRITE){
            //cerr<<"push op aio write:"<<msg->opid<<endl;
            pdc->OpFindClient(msg);
            prbd = reinterpret_cast<BackendClient::RbdVolume *>(msg->pop_volume());
            if(!prbd){
            cerr<<"get NULL volume"<<endl;
            //delete op;
            assert(0);
            continue;
            }
            u64 bufsize = msg->data.len;
            char * buf = (char *)msg->originbuf;
            for(int i = 0;i < msg->data.chunksize;i++){
                //memset(op->data.pdata, 6, op->data.len);
                u64 size = bufsize > CHUNKSIZE ? CHUNKSIZE:bufsize;
                
                simpledata * pdata = pdc->slab.getaddbyindex(msg->data.indexlist[i]);
                //TODO: WRITE
                ::memcpy(pdata, &(buf[i*CHUNKSIZE]), size);
                //r = do_op(,pdata);
                bufsize -= CHUNKSIZE;
            }
            p_pipe = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(prbd->mq[SENDMQ]);
            r = p_pipe->push(msg);
            if(r< 0){
                cerr<<"push op aio write:"<<msg->opid<<" failed"<<endl;
                delete msg;
                continue;
            }
        }
        
        delete msg;
    }//while()
		
    }//while(1)
    return 0;
}


void* PdcClient::Finisherthreads::_process()
{
    PdcClient *pdc = (PdcClient *)pc;
    Msginfo *msg;
    Msginfo * op;
    u64 s = 0;
    pdcPipe::PdcPipe<Msginfo>* p_pipe;
    cerr<<"listen thread  start"<<endl;
    
    while(1){
        if(stop()) continue;
        if(!p_pipe) {
            p_pipe = pdc->ackmq;
            continue;
        }
	 //cerr<<"to start open"<<endl;
        op = p_pipe->pop();
        if(op){
            msg = new Msginfo();
            msg->copy(op);
            p_pipe->clear();
        }else{
            continue;
        }

        if(msg){
            msg->dump("client finish tp op");
            if(msg->opcode == ACK_MEMORY){
                msg->opcode = PDC_AIO_WRITE;
                //p_pipe->clear();
                
                msg->dump("get memory ack, todo RW");
                //pdc->OpFindClient(msg);
                pdc->iolock.lock();
                pdc->ops.push_back(msg);
                //pdc->opcond.Signal();
                pdc->iolock.unlock();
                
            }
            if(msg->opcode == RW_W_FINISH){
                pdc->msglock.lock();
                pdc->msgop.push_back(msg);
                //pdc->msgcond.Signal();
                pdc->msglock.unlock();
                
            }
             
            /*
            if(msg->opcode == RW_W_FINISH){
                cerr<<"write op:["<<msg->opid<<"] return:"<<msg->getreturnvalue()<<endl;
                PdcCompletion *c = reinterpret_cast<PdcCompletion*>(msg->data.c);
                if(c && c->callback){
                    ///c->callback(c->comp, c->callback_arg);
                    c->complete(msg->return_code);
                    //todo put shmmemory keys .  
                    vector<u64> index(op->data.indexlist,op->data.indexlist+sizeof(op->data.indexlist)/sizeof(u64));
                    pdc->release_shmkey(index);

                }
                delete msg;
            }
            */

	 }
        //free shared memory
        
        //TODO: here ,we connect rados  and write rbd

        /*
        if(0){
            pthread_mutex_lock(&pdc->finimutex);
            if(pdc->finishop.empty()){
                pthread_mutex_unlock(&pdc->finimutex);
                continue;
            }
            msg = pdc->finishop.front();
            pdc->finishop.pop_front();
            pthread_mutex_unlock(&pdc->finimutex);
        }
        */
    }

return NULL;

}


void* PdcClient::Msgthreads::_process()
{
    int r;
    map<string , u64> sum;
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
    BackendClient::RbdVolume *prbd;
    PdcClient *pdc = (PdcClient *)pc;
    PdcCompletion *c;
    cerr<<"IOthread "<<pthread_self()<<" start"<<endl;
    
    while(1){
        if(stop() ) continue;

        pdc->msglock.lock();
        //pdc->msgcond.wait(pdc->msglock);
        list<Msginfo *> oplist;
        oplist.swap(pdc->msgop);
       pdc->msglock.unlock();

        while(!oplist.empty()){
        Msginfo *msg =  oplist.front();
        oplist.pop_front();
        
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
            //assert(msg->client.cluster == "ceph");
            client[msg->client.pool] = msg->client.volume;
            //perf.inc();
            prbd = reinterpret_cast<BackendClient::RbdVolume *>(msg->volume); 

            switch(msg->opcode){
            case OPEN_RADOS:
                cerr<<"do not come here rados"<<msg->opid<<endl;
                break;
            case OPEN_RBD:
                 cerr<<"do not come here rbd"<<msg->opid<<endl;
                 break;
            case PDC_AIO_WRITE:
            assert(prbd);
                msg->opcode =GET_MEMORY;
                //list<u64> listadd ;
                //TODO:SET START TIME
                msg->getopid();
                //cerr<<"start to get memory"<<endl;
                p_pipe = (pdcPipe::PdcPipe<Msginfo>*)prbd->mq[SENDMQ]; 
                assert(p_pipe);
                r = p_pipe->push(msg);
                if(r< 0){
                    cerr<<"=============get memory failed"<<endl;
                    msg->dump(" msg push failed" );
                    //delete msg;
                    //break;
                }
                break;
                //delete msg;
                //TODO:SET END TIME
            case PDC_AIO_READ:

                break;
            case ACK_MEMORY:
            //send msg and wait for ack:
                assert(0);
                break;
            case RW_W_FINISH:
                //cerr<<"write op:["<<msg->opid<<"] return:"<<msg->getreturnvalue()<<endl;
                c = reinterpret_cast<PdcCompletion*>(msg->data.c);
                if(c && c->callback){
                    ///c->callback(c->comp, c->callback_arg);
                    c->complete(msg->return_code);
                    c->release();
                }
                break;
            default:
                cerr<<"other code? "<<msg->opcode<<endl;
                break;
				
        }
        delete msg;
        p_pipe->clear();
  
        }
    }


}

return NULL;
}

int PdcClient::init()
{
    int ret ;
    int r;
    int i;
    pthread_t id;
    string state("init: ");
    cerr<<state<<"start thread:"<<endl;

    BackendClient* pceph = new BackendClient("ceph","/etc/ceph/ceph.conf", &msgop, &msglock._mutex);
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

    //pthread_cond_init(&opcond);
    //pthread_mutex_init(&iomutex,NULL);
    iothread = new PdcClient::Iothreads("IO-threadpool", this);
    iothread->init(1);

    //pthread_cond_init(&msgcond);
    //pthread_mutex_init(&msgmutex,NULL);
    msgthread = new Msgthreads("MSG-threadpool",this);
    msgthread->init(1);

    //pthread_cond_init(&listencond);
    //pthread_mutex_init(&finimutex,NULL);
    listen = new Finisherthreads("Finisher threadpool", this);
    listen->init(1);	
    ops.clear();
    listenop.clear();
    msgop.clear();

    listen->start();
    iothread->start();
    msgthread->start();
    
    cerr<<"pdcclient init over"<<endl;
    return 0;
}


void PdcClient::OpFindClient(Msginfo *&op)
{
    string pool(op->client.pool);
    string volume(op->client.volume);
    map<string,string> opclient;
    opclient[pool] = volume;
    string backendname("ceph");
    BackendClient *backend = clusters[backendname];
    if(!backend){
        cerr<<"find no backend now for:"<<backendname<<endl;
        assert(0);
        return;
    }
    op->volume =  backend->findclient( &opclient);
    assert(op->volume);

}



