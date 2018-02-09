#include "rbd/librbd.h"
#include "rados/librados.h"


//#include "type.h"
#include "pdcclient.hpp"
#include <sys/prctl.h>

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
	
    prctl(PR_SET_NAME, "IOthread");
    cerr<<"IOthread "<<pthread_self()<<" start"<<endl;
    
    while(1){
        if(stop()) continue;
        
        pdc->iolock.lock();

        //pdc->opcond.wait( pdc->iolock);
        list<Msginfo*>oplist;
        oplist.swap(pdc->ops);
        pdc->iolock.unlock();

        while(!oplist.empty()){
        Msginfo *msg = oplist.front();
        oplist.pop_front();
        msg->dump("iothread: client io tp op");
        if(msg->opcode == PDC_AIO_WRITE){
            pdc->OpFindClient(msg);
            prbd = reinterpret_cast<BackendClient::RbdVolume *>(msg->pop_volume());
            if(!prbd){
            cerr<<"get NULL volume"<<endl;
            assert(0);
            continue;
            }
            u32 copy_loc = 0;
            u64 bufsize = msg->u.data.len;
            char *buf = (char *)msg->originbuf;
            for(int i = 0;i < msg->u.data.chunksize; i++){
                int unit_size;
                u32 copy_size;
                char *pdata;
				
                assert(bufsize > 0);
                unit_size = pdc->slab.get_unit_size(msg->u.data.indexlist[i]);
                assert(unit_size != -1);
                copy_size = bufsize > unit_size ? unit_size : bufsize;                
                pdata = (char *)pdc->slab.getaddbyindex(msg->u.data.indexlist[i]);
                assert(pdata != NULL);
                //TODO: WRITE
                ::memcpy(pdata, buf + copy_loc, copy_size);
                bufsize -= copy_size;
                copy_loc += copy_size;
            }
            p_pipe = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(prbd->mq[SENDMQ]);
            r = p_pipe->push(msg);
            if(r< 0){
                cerr<<"push op aio write:"<<msg->opid<<" failed"<<endl;
            }
        }
     
        pdc->msg_pool.free(msg);
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
    int r;
    pdcPipe::PdcPipe<Msginfo>* p_pipe;
    
    prctl(PR_SET_NAME, "Listenthread");
    cerr<<"listen thread  start"<<endl;
    
    while(1){
        if(stop()) continue;
        if(!p_pipe) {
            p_pipe = pdc->ackmq;
            continue;
        }
	 //cerr<<"to start open"<<endl;
        msg = pdc->msg_pool.malloc();
        assert(msg != NULL);
        r = p_pipe->pop(msg);
        if(0 != r){
		    //cerr<<"pop size is:"<<r<<endl;
			pdc->msg_pool.free(msg);
			continue;
        }
        msg->init_after_read();

        if(msg){
            msg->dump("client finish tp op");
            switch(msg->opcode){
            case ACK_MEMORY:
                msg->opcode = PDC_AIO_WRITE;
                pdc->msglock.lock();
                pdc->msgop.push_back(msg);
                //pdc->msgcond.Signal();
                pdc->msglock.unlock();
                pdc->msgcond.Signal();
                break;
            case RW_W_FINISH:
                pdc->msglock.lock();
                pdc->msgop.push_back(msg);
                pdc->msgcond.Signal();
                pdc->msglock.unlock();
                break;
            case RW_R_FINISH:
                pdc->msglock.lock();
                pdc->msgop.push_back(msg);
                pdc->msgcond.Signal();
                pdc->msglock.unlock();
                break;
             default:
                cerr<<"finish op:"<<msg->opcode<<endl;
                msg->enabledump();
                msg->dump("get error code ");
                msg->disabledump();
                assert(0);
                break;
        }
    }

}
return NULL;

}


void* PdcClient::Msgthreads::_process()
{
    int r;
    map<string , u64> sum;
    u64 bufsize, size;
    u32 copy_loc = 0;
    char * buf;
    simpledata * pdata;
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
    BackendClient::RbdVolume *prbd;
    PdcClient *pdc = (PdcClient *)pc;
    PdcCompletion *c;
	
    prctl(PR_SET_NAME, "MSGthread");
    cerr<<"MSGthread "<<pthread_self()<<" start"<<endl;
    
	
    while(1){
        if(stop() ) continue;

        pdc->msglock.lock();
        //pdc->msg_working++;
        /*如果队列为0，等待*/
        if(pdc->msgop.empty() )
            pdc->msgcond.wait(pdc->msglock);
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
            client[msg->u.mgr.client.pool] = msg->u.mgr.client.volume;
            //perf.inc();
            //prbd = reinterpret_cast<BackendClient::RbdVolume *>(msg->pop_volume()); 
            
            switch(msg->opcode){
            case OPEN_RADOS:
                cerr<<"do not come here rados"<<msg->opid<<endl;
                break;
            case OPEN_RBD:
                 cerr<<"do not come here rbd"<<msg->opid<<endl;
                 break;
            case PDC_AIO_PREWRITE:
                //assert(prbd);
                msg->opcode =GET_MEMORY;
                //TODO:SET START TIME
                msg->getopid();
                //p_pipe = (pdcPipe::PdcPipe<Msginfo>*)prbd->mq[SENDMQ]; 
                p_pipe = (pdcPipe::PdcPipe<Msginfo>*)pdc->mq[SENDMQ]; 
                assert(p_pipe);
                r = p_pipe->push(msg);
                if(r< 0){
                    cerr<<"=============get memory failed"<<endl;
                    msg->dump(" msg push failed" );
                }
                break;
            case PDC_AIO_WRITE:  
                copy_loc = 0;
                bufsize = msg->u.data.len;
                buf = (char *)msg->originbuf;
                for (int i = 0;i < msg->u.data.chunksize; i++) {
                    int unit_size;
                    u32 copy_size;
                    char *pdata;
					
                    assert(bufsize > 0);
                    unit_size = pdc->slab.get_unit_size(msg->u.data.indexlist[i]);
                    assert(unit_size != -1);
                    copy_size = bufsize > unit_size ? unit_size : bufsize;
                    pdata = (char *)pdc->slab.getaddbyindex(msg->u.data.indexlist[i]);
                    assert(pdata != NULL);
                    ::memcpy(pdata, buf + copy_loc, copy_size);
                    bufsize -= copy_size;
                    copy_loc += copy_size;
                }
                p_pipe = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(pdc->mq[SENDMQ]);
                r = p_pipe->push(msg);
                if(r< 0){
                    cerr<<"push op aio write:"<<msg->opid<<" failed"<<endl;
                }

                break;
                //TODO:SET END TIME
            case PDC_AIO_READ:
                //TODO:SET START TIME
                msg->getopid();
                p_pipe = (pdcPipe::PdcPipe<Msginfo>*)pdc->mq[SENDMQ]; 
                assert(p_pipe);
                r = p_pipe->push(msg);
                if(r< 0){
                    cerr<<"=============get memory failed"<<endl;
                    msg->dump(" msg push failed" );
                }
                break;
            case ACK_MEMORY:
            //send msg and wait for ack:
                assert(0);
                break;
            case RW_W_FINISH:
                //assert(prbd);
                //cerr<<"write op:["<<msg->opid<<"] return:"<<msg->getreturnvalue()<<endl;
                c = reinterpret_cast<PdcCompletion*>(msg->u.data.c);
                if(c && c->callback){
                    ///c->callback(c->comp, c->callback_arg);
                    c->complete(msg->return_code);
                    c->release();
                }
                break;
             case RW_R_FINISH:
			 	//TODO,if msg->return_code is not zero,then we shouldn't copy shm to buf
                 copy_loc = 0;
                 bufsize = msg->u.data.len;
                 buf = (char *)msg->originbuf;
                 for (int i = 0;i < msg->u.data.chunksize; i++) {
                     int unit_size;
                     u32 copy_size;
                     char *pdata;
                     assert(bufsize > 0);
                     unit_size = pdc->slab.get_unit_size(msg->u.data.indexlist[i]);
                     assert(unit_size != -1);
                     copy_size = bufsize > unit_size ? unit_size : bufsize;
                     pdata = (char *)pdc->slab.getaddbyindex(msg->u.data.indexlist[i]);
                     assert(pdata != NULL);
                     ::memcpy(buf + copy_loc, pdata, copy_size);
                     bufsize -= copy_size;
                     copy_loc += copy_size;
				 }

                //cerr<<"read op:["<<msg->opid<<"] return:"<<msg->getreturnvalue()<<endl;
                c = reinterpret_cast<PdcCompletion*>(msg->u.data.c);
                if(c && c->callback){
                    ///c->callback(c->comp, c->callback_arg);
                    c->complete(msg->return_code);
                    c->release();
                }
				
                // whether send cmd to server to put sharedmemory
                msg->opcode = PUT_SHM;
                msg->getopid();
                //pdc->OpFindClient( msg);
                //prbd = reinterpret_cast<BackendClient::RbdVolume *>(msg->pop_volume());
                p_pipe = (pdcPipe::PdcPipe<Msginfo>*)pdc->mq[SENDMQ]; 
                assert(p_pipe);
                r = p_pipe->push(msg);
                if(r< 0){
                    cerr<<"=============send put memory failed"<<endl;
                    msg->dump(" msg push failed" );
                }
                break;
            
            default:
                cerr<<"other code? :"<<msg->opcode<<endl;
                break;
				
        }
        //delete msg;
        pdc->msg_pool.free(msg);
        //p_pipe->clear();
  
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

    BackendClient* pceph = new BackendClient("ceph","/etc/ceph/ceph.conf", 
						&msgop, &msglock._mutex, &msg_pool);
    clusters["ceph"] = pceph;
    //pceph->setqueuefn(&enqueue);
    ret = slab.Init();
    if(ret < 0){
        cerr<<"slab init faled:"<<ret<<endl;
    }

    map<string, void *>mqs;
    mqs.clear();
    
    r = pdcPipe::createclientqueues(mqs, true);  //false means to create send pipe
    if(r< 0){
        cerr<<"pdc client init recvmq failed"<<endl;
        return r;
    }
    
    ackmq = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>* >( mqs[RECVMQ]);
    sendmq = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>* >( mqs[SENDMQ]);
    if(MULTIPIPE){
        mq.swap(mqs);
    }else{
        mq.insert(pair<string,void *>(SENDMQ, (void*)&msgmq));
        mq.insert(pair<string,void *>(RECVMQ, (void*)ackmq));
    }
    //msgmq = new wp::Pipe::Pipe(PIPEKEY, MEMQSEM, PIPEREAD,wp::Pipe::SYS_t::PIPESERVER);
    ret = msgmq.Init();
    if(ret < 0){
        cerr<<"msgmq init faled:"<<ret<<endl;
    }

    /*
    iothread = new PdcClient::Iothreads("IO-threadpool", this);
    iothread->init(1);
    */
    msgthread = new Msgthreads("MSG-threadpool",this);
    msgthread->init(2);

    listen = new Finisherthreads("Finisher threadpool", this);
    listen->init(1);	
	
    //ops.clear();
    listenop.clear();
    msgop.clear();

    listen->start();
    //iothread->start();
    msgthread->start();
    
    cerr<<"pdcclient init over"<<endl;
    return 0;
}


void PdcClient::OpFindClient(Msginfo *&op)
{
    string pool(op->u.mgr.client.pool);
    string volume(op->u.mgr.client.volume);
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


int PdcClient::aio_write(BackendClient::RbdVolume *prbd, u64 offset, size_t len,const char *buf,
								PdcCompletion *c)
{

    Msginfo *msg  = msg_pool.malloc();
    msg->default_init();
    
    //msg->getopid();
    msg->opcode = PDC_AIO_PREWRITE;
    strcpy(msg->u.mgr.client.pool, prbd->rados->GetName());
    strcpy(msg->u.mgr.client.volume, prbd->GetName());
    msg->originbuf = buf;
    msg->u.data.offset = offset;
    msg->u.data.len = len;
    msg->u.data.c = (void *)c;
    msg->insert_volume((void *)prbd);
    msg->dump("rbd aio write");

    /*add request to completion*/
    c->add_request();
    enqueue(msg);

    return 0;
}


int PdcClient::aio_read(BackendClient::RbdVolume *prbd, u64 offset, size_t len,const char *buf, PdcCompletion *c)
{

    Msginfo *msg  = msg_pool.malloc();
    msg->default_init();
    //msg->getopid();
    msg->opcode = PDC_AIO_READ;
    strcpy(msg->u.mgr.client.pool, prbd->rados->GetName());
    strcpy(msg->u.mgr.client.volume, prbd->GetName());
    msg->originbuf = buf;
    msg->u.data.offset = offset;
    msg->u.data.len = len;
    msg->u.data.c = (void *)c;
    //op->volume = (void *)prbd;
    msg->insert_volume((void *)prbd);
    msg->dump("rbd aio read");
    
    /*add request to completion*/
    c->add_request();
    enqueue(msg);
    /*
    pthread_mutex_lock(prbd->rados->ceph->_mutex);
    prbd->rados->ceph->_queue->push_back(msg);
    pthread_mutex_unlock(prbd->rados->ceph->_mutex);
    */
    return 0;
}



