//#include "rbd-bak/librbd.h"
//#include "rados-bak/librados.h"


//#include "type.h"
#include "pdcserver.hpp"
#include <sys/prctl.h>
//#include "backend_ceph.hpp"

Pdcserver*pdc_server_mgr;

int GetMemory(Pdcserver *pdc, Msginfo *msg)
{
    int r;
    CephBackend::RbdVolume *vol;
    pdcPipe::PdcPipe<Msginfo>::ptr pipe;

    pdc->OpFindClient(msg);
    r = pdc->getshm(msg->data.len, msg->data.indexlist);
    if(r <= 0){
        cerr<<"get memory failed"<<endl;
        //TODO : need more todo
       return r;
    }
    msg->data.chunksize = r;
    msg->opcode = ACK_MEMORY;
    vol = reinterpret_cast<CephBackend::RbdVolume *>(msg->volume);
    pipe =reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(vol->mq[SENDMQ]);
    assert(pipe);
    r = pipe->push(msg);
    if(r < 0){
        msg->dump("push failed");
        cerr<<"pipe push msg:"<<msg->opid<<" failed"<<endl;
        return r;
    }

    return 0;
}


void pdc_callback(rbd_completion_t cb, void *arg)
{
    int r;
    int n;
    u32 bufsize = 0;
    u32 lengh = 0;
    u32 pos = 0;
    char *buf;

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
        switch(op->opcode){
        case PDC_AIO_WRITE:
            op->opcode =  RW_W_FINISH;
            //todo put shmmemory keys .  
            
            if(pdc){
                vector<u64> index(op->data.indexlist,op->data.indexlist+op->data.chunksize);
                n = pdc->slab.put(index);
                if(n < 0 ){
                    cerr<<"shm->put falied:"<<n <<" and index is:"<<index.size()<<endl;
                    //return ;
                }
            }
            break;

        case PDC_AIO_READ:
            op->opcode =  RW_R_FINISH;
            buf = (char *)op->op;
            assert(buf);
            if(pdc){
                 // copy buffer
                 
                //vector<u64> index(op->data.indexlist,op->data.indexlist+op->data.chunksize);
                n = pdc->slab.get(op->data.len, op->data.indexlist);
                if(n < 0 ){
                    cerr<<"callback slab->get failed:"<< n <<" and index is:"<<op->data.chunksize<<endl;
                    //return ;
                    break;
                }
                op->data.chunksize = n;
                bufsize = op->data.len;
                lengh = 0;
                pos = 0;
                
                for(int i = 0;i < op->data.chunksize;i++){
                //memset(op->data.pdata, 6, op->data.len);
                //simpledata * pdata = pdc->slab.getaddbyindex(op->data.indexlist[i]);
                char *pdata = (char *)pdc->slab.getaddbyindex(op->data.indexlist[i]);
                //TODO: WRITE
                //lengh = bufsize > CHUNKSIZE ? CHUNKSIZE:bufsize;
                lengh = bufsize;
                ::memcpy((char *)pdata, buf + pos,  lengh);
                bufsize -= lengh;
                pos += lengh;
                }

            }
            if(buf){
                ::free(buf);
                op->op = NULL;
                buf = NULL;
            }
            break;
        default:
            op->dump("callback error code ");
            break;

        }
        pdcPipe::PdcPipe<Msginfo>*p_pipe = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(prbd->mq[SENDMQ]);
        r = p_pipe->push(op);
        if(r < 0){
            cerr<<"callback to push pipe error:"<<r<<endl;
        }
        //TO DELETE OP
        
    }
    //delete op;
	pdc->msg_pool.free(op);
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
    u64 off = 0;
    u32 bufsize = 0;
    u32 write_lengh;
    int unit_size;
    char *buf;
    static bool flag = false;
    Pdcserver *pdc = (Pdcserver *)server;
    CephBackend::RbdVolume *vol;

    prctl(PR_SET_NAME, "IOthread");
    cerr<<"IOthread "<<pthread_self()<<" start"<<endl;
    
    while(1){
        if(stop()) continue;
        
        pdc->iolock.lock();
        while(pdc->ops.empty())
            pdc->iocond.wait(pdc->iolock);
        list<Msginfo *> oplist;
        oplist.swap(pdc->ops);
        pdc->iolock.unlock();

        while(!oplist.empty()){
        Msginfo *op = oplist.front();
        oplist.pop_front();

        op->dump("server io tp op");
        pdc->OpFindClient(op);
        vol = reinterpret_cast<CephBackend::RbdVolume *>(op->volume);
        if(!vol){
            op->dump("get NULL volume");
            cerr<<"get NULL volume"<<endl; 
            continue;
        }
        sum++;
        if(SERVER_IO_BLACKHOLE){   //black hole
            if(!flag){
                flag = true;
                cerr<<"********server start to use black hole*******"<<endl;
            }
        }
        rbd_completion_t comp;
        switch(op->opcode){
        // aio write
        case PDC_AIO_WRITE:
            if (!SERVER_IO_BLACKHOLE) {
                off = op->data.offset;
                bufsize = op->data.len;
                flag = false;
                vol->do_create_rbd_completion(op, &comp);
                /*TODO:deal with muti index,
                           when multi aio write is done,we call function pdc_callback*/
                for (int i = 0; i < op->data.chunksize; i++) {            
                    assert(bufsize > 0);
                    unit_size = pdc->slab.get_unit_size(op->data.indexlist[i]);
                    assert(unit_size != -1);
                    char *pdata = (char *)pdc->slab.getaddbyindex(op->data.indexlist[i]);
                    assert(pdata != NULL);
                    write_lengh = bufsize > unit_size ? unit_size : bufsize;
                    vol->do_aio_write(op, off, write_lengh, (char *)pdata, comp);   
                    bufsize -= write_lengh;
                    off += write_lengh;
                }
            } else {
                op->ref_inc();
                pdc_callback(NULL, op);
               //continue;
            }
            break;
        // aio read
        case PDC_AIO_READ:
            //rbd_completion_t comp;
            
            off = op->data.offset;
            bufsize = op->data.len;
            //buf = NULL;
            buf = (char *)malloc(op->data.len);
            op->op = (void *)buf;
        if(!SERVER_IO_BLACKHOLE){
            flag = false;
            vol->do_create_rbd_completion(op, &comp);
            vol->do_aio_read(op,  op->data.offset, op->data.len, buf, comp);   
        }else{
            op->ref_inc();
            pdc_callback(NULL, op);

        }
            break;		
        default:
            assert(0);
            break;
       }
    }
    }
    return 0;
}



int handle_listen_events(Pdcserver *pdc, Msginfo* op)
{
    static u64 sum =0;
    int r;
        if(op)
            op->dump("listen thread get op");
        switch(op->opcode){
        case PDC_AIO_WRITE:
            pdc->iolock.lock();
            pdc->ops.push_back(op);
            pdc->iocond.Signal();
            pdc->iolock.unlock();
            
            break;
        
        case PDC_AIO_READ:
            pdc->iolock.lock();
            pdc->ops.push_back(op);
            pdc->iocond.Signal();
            pdc->iolock.unlock();
            
            break;
        case GET_MEMORY:
            if(DIRECT_ACK){
                r = GetMemory(pdc,op);
                if(r < 0 ){
                    cerr<<"get memory failed:"<<r <<endl;
                }
            }else{
                pdc->msglock.lock();
                pdc->msgop.push_back(op);
                pdc->msgcond.Signal();
                pdc->msglock.unlock();
            }
            break;
        default:  //mgr cmd
            pdc->msglock.lock();
            pdc->msgop.push_back(op);
            pdc->msgcond.Signal();
            pdc->msglock.unlock();
            
            break;
        }
        sum++;



}
int del_event(int epfd, int listen_fd)
{
    struct epoll_event ev;
    //ev.status = 0;

    if (::epoll_ctl(epfd, EPOLL_CTL_DEL, listen_fd, NULL) < 0){
        cerr<<"epoll set delete error: fd="<<ev.data.fd<<endl;  
        return -1;  
    }else {  
        cerr<<"listening PIPE delete epoll success"<<endl; 
    }
    return 0;

}
int add_event(Pdcserver *pdc, int epfd, struct epoll_event &ev, Msginfo *op, int listen_fd)
{
    int r;
    ev.events = EPOLLIN;
    if(listen_fd != 0)
        ev.data.fd = listen_fd;
    else{
        if(op){
            CephBackend::RbdVolume *vol;
            pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
            pdc->OpFindClient( op);
            vol = reinterpret_cast<CephBackend::RbdVolume *>(op->volume);
            p_pipe =reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(vol->mq[RECVMQ]);
            ev.data.fd = p_pipe->GetFd();
            if(ev.data.fd < 0 ){
                cerr<<"add_event fd is "<< ev.data.fd<<" maybe not opened"<<endl;
                return  -1;
            }
        }
    }
    if (::epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0){
        cerr<<"epoll set insertion error: fd="<<ev.data.fd<<endl;  
        return -1;  
    }else {  
        cerr<<"listening PIPE insert epoll success"<<endl; 
	
    }
    return 0;
}
void* Pdcserver::Finisherthreads::_process()
{
    int r;
    int epfd;
    int curfds = 0;
    int fds, n;
    int listenfd;
    u64 sum = 0;
    struct epoll_event ev;
    struct epoll_event events[EPOLLSIZE];
    Pdcserver *pdc = (Pdcserver *)server;
    CephBackend::RbdVolume *prbd;
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;

    prctl(PR_SET_NAME, "Listenthread");
    cerr<<"listen thread  start"<<endl;
    r = pdc->msgmq.openpipe();
    if(r< 0){
        cerr<<"listen pipe open failed "<<r<<endl;
        return NULL;
    }
    epfd = ::epoll_create(EPOLLSIZE);  
    if(epfd < 0 ){
        cerr<<"epoll create failed :"<<epfd<<endl;
        //return NULL;
    }
    listenfd = pdc->msgmq.GetFd();
    add_event(pdc,epfd, ev, NULL, listenfd);
    curfds++;
	
    while(1){
        if(stop()) continue;

        fds = epoll_wait (epfd, events, EPOLLSIZE, -1);
        if(fds == -1){
            if(errno == EINTR) continue;
            else {
                cerr<<"epoll wait error:"<<strerror(errno)<<endl;;
                break;
            }
        }
        int tfd;
        int bufsize = sizeof(Msginfo);
        for(n = 0; n<fds ;n ++){
            tfd = events[n].data.fd;    //tmp fd
            /*if all client use one fifo to write ,then tfd== listenfd ,just do read
            * if every client use it's own fifo fd, then tfd == listenfd ,
            * we need to check if need to add new fd to epoll
            */
            //Msginfo *op = new Msginfo();
            Msginfo *op = pdc->msg_pool.malloc();
            r = ::read(tfd, op, bufsize);
            
            if(( tfd == listenfd )&& (events[n].events & EPOLLIN)){  //
                if(op->opcode == PDC_ADD_EPOLL){ // MULTI model to add epoll listen
                    op->dump("pdc add epoll");
                    if(MULTIPIPE){
                       add_event(pdc, epfd, ev, op, 0);
                    }
                    curfds++;
                     //ev.data.fd = ;
                    //delete op;
                    pdc->msg_pool.free(op);
                }else{
                  if(r == bufsize){
                       op->init_after_read();
                       r = handle_listen_events(server,op);
                  }else{
                       cerr<<"msg pipe read buf  is:"<<r<<" but should be:"<<bufsize<<endl;
					   pdc->msg_pool.free(op);
                  }
                }
            }else{
                if(r == bufsize){
                    op->init_after_read();
                    r = handle_listen_events(server,op);
                }else{
                    // need  delete ?
                    //cerr<<"muliti pipe read buf  is:"<<r<<" but should be:"<<bufsize<<endl;
                    if(tfd != listenfd)
                        del_event(epfd, tfd);
                    else{
                    /*all client is shutdown ,to free cpu*/
                        usleep(100);
                    }
                    pdc->msg_pool.free(op);
                }
                //cerr<<" fds:"<<fds <<" now is:"<<n<<" fd :"<<tfd<<endl;
            }

        }
        
        //op->dump("server finish tp op");
        //cerr<<" get a finish op ,do pop"<<endl;
    }

return 0;

}

void* Pdcserver::Msgthreads::_process()
{
    int r;
    int n;
    u64 sum = 0;
    CephBackend::RbdVolume *vol;
    pdcPipe::PdcPipe<Msginfo>::ptr pipe;

    Pdcserver *pdc = (Pdcserver *)server;

    prctl(PR_SET_NAME, "MSGthread");
    cerr<<"MSGthread "<<pthread_self()<<" start"<<endl;

    //pdc->msglock.lock();
    while(1){
        if(stop() ) continue;
        pdc->msglock.lock();
        while(pdc->msgop.empty())
            pdc->msgcond.wait(pdc->msglock);
        list<Msginfo *> oplist;
        oplist.swap(pdc->msgop);
        pdc->msglock.unlock();
        
        while(!oplist.empty()){
            Msginfo *msg =oplist.front();
            oplist.pop_front();
        if(msg){
            sum++;
            assert(msg);
            msg->dump("server msg tp op");
            std::map<string,string> client;
            //assert(msg->client.cluster == "ceph");
            client[msg->client.pool] = msg->client.volume;
            //perf.inc();
            switch(msg->opcode){
            case OPEN_RADOS:
            {
                pdc->register_connection(msg);
                break;
            }
            case OPEN_RBD:
            {
                r = pdc->register_vm(client, msg);
                if(r < 0){
                    cerr<<"register vm failed ret = "<<r <<endl;
                    //delete msg;
                    //continue;
                }
                break;
            }
            case PDC_AIO_STAT:
                //pdc->OpFindClient(msg);
                //vol = reinterpret_cast<CephBackend::RbdVolume *>(msg->volume);
                //pipe =reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(vol->mq[SENDMQ]);


                break;
            case GET_MEMORY:
            {
                pdc->OpFindClient(msg);
                r = pdc->slab.get(msg->data.len, msg->data.indexlist);
				//cerr << "index: " << msg->data.indexlist[0] << endl;
                if(r <= 0){
                    cerr<<"get memory failed"<<endl;
					//TODO : need more todo
                    break;
                }
                msg->data.chunksize = r;
                msg->opcode = ACK_MEMORY;
                pdcPipe::PdcPipe<Msginfo>::ptr pipe;
                vol = reinterpret_cast<CephBackend::RbdVolume *>(msg->volume);
                pipe =reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(vol->mq[SENDMQ]);
                assert(pipe);
                r = pipe->push(msg);
                if(r < 0){
                    msg->dump("push failed");
                    cerr<<"pipe push msg:"<<msg->opid<<" failed"<<endl;
                    break;
                }
                break;
            }
            case PUT_SHM:
                n = pdc->release_shmkey(msg);
                if(n < 0 ){
                    cerr<<"shm->put falied:"<<n <<" and index is:"<<msg->data.chunksize<<endl;
                }            
			
            break;
            default:
            {
                cerr<<"opcode error:"<<msg->opcode<<endl;
                assert(0);
                break;
            }

         }
        }
        //delete msg;
	    pdc->msg_pool.free(msg);
       }

       //pdc->msglock.lock();
       //pdc->msgcond.wait(pdc->msglock);
      //}
    }

    //pdc->msglock.unlock();
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

    ret = slab.Init();
    if(ret < 0){
        cerr<<"slab init faled:"<<ret<<endl;
        return -1;
    }
    
    ret = msgmq.Init();
    if(ret < 0){
        cerr<<"msgmq init faled:"<<ret<<endl;
    }

    iothread = new Pdcserver::Iothreads("IO-threadpool", this);
    iothread->init(2);

    msgthread = new Msgthreads("MSG-threadpool",this);
    msgthread->init(2);

    listen = new Finisherthreads("Finisher threadpool", this);
    listen->init(1);	

    ops.clear();
    finishop.clear();
    msgop.clear();

    listen->start();
    iothread->start();
    msgthread->start();
    
    cerr<<"pdcserver init over"<<endl;
    return 0;
}
/*
int Pdcserver::getshm(u32 size, u64* sum)
{
    return slab.get( size, sum);
}
*/
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

void Pdcserver::wait_to_shutdown()
{
    
    iothread->join();     
    msgthread->join();
    listen->join();

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
    /*
    r = 0;
    while(1){
       r++;
	sleep(1);
    }
    */
    server->wait_to_shutdown();
    //todo : shutdown
    return 0;
}

