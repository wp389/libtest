
#include "pdcserver.hpp"
#include <sys/prctl.h>

Pdcserver*pdc_server_mgr;

int GetMemory(Pdcserver *pdc, Msginfo *msg)
{
    int r;
    CephBackend::RbdVolume *vol;
    pdcPipe::PdcPipe<Msginfo>::ptr pipe;

    pdc->OpFindClient(msg);
    r = pdc->getshm(msg->u.data.len, msg->u.data.indexlist);
    if(r <= 0){
        cerr<<"get memory failed"<<endl;
        //TODO : need more todo
       return r;
    }
    msg->u.data.chunksize = r;
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
                vector<u32> index(op->u.data.indexlist,op->u.data.indexlist+op->u.data.chunksize);
                n = pdc->slab.put(index);
                if(n < 0 ){
                    cerr<<"shm->put falied:"<<n <<" and index is:"<<index.size()<<endl;
                    //return ;
                }
            }
            break;

        case PDC_AIO_READ:
            op->opcode =  RW_R_FINISH;
            break;
        case PDC_AIO_FLUSH:
            op->opcode =  RW_R_FINISH;;
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
        if (!pdc->io_split && op->u.data.chunksize > 1) {
            assert(op->u.data.buffer);
            ::free((void *)op->u.data.buffer);
            op->u.data.buffer = NULL;
        }
        pdc->msg_pool.free(op);
    }
    //delete op;
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
    u32 write_length;
    u32 read_length;
    int unit_size;
    int num_index = 0;
    u32 copy_loc = 0;
    u32 copy_length;
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
                flag = false;
                off = op->u.data.offset;
                bufsize = op->u.data.len;
                if (pdc->io_split ||
                    (!pdc->io_split && op->u.data.chunksize == 1)) {
                    /*issue multi sub-IO when getting muti shm unit index*/
                    for (int i = 0; i < op->u.data.chunksize; i++) {            
                        assert(bufsize > 0);
                        unit_size = 
                            pdc->slab.get_unit_size(op->u.data.indexlist[i]);
                        assert(unit_size != -1);
                        char *pdata = (char *)
                            pdc->slab.getaddbyindex(op->u.data.indexlist[i]);
                        assert(pdata != NULL);
                        write_length = bufsize > unit_size ? unit_size : bufsize;
                        vol->do_create_rbd_completion(op, &comp);
                        vol->do_aio_write(op, off, write_length, pdata, comp);   
                        bufsize -= write_length;
                        off += write_length;
                    }
                } else {
                    /*gather multi shm to a larger buffer, and issue a single IO*/
                    char *tmp_buffer;
                    tmp_buffer = (char *)::malloc(op->u.data.len);
                    assert(tmp_buffer);
					op->u.data.buffer = tmp_buffer;
                    copy_loc = 0;
                    for (int i = 0; i < op->u.data.chunksize; i++) {            
                        assert(bufsize > 0);
                        unit_size = 
                             pdc->slab.get_unit_size(op->u.data.indexlist[i]);
                        assert(unit_size != -1);
                        char *pshm = (char *)
                            pdc->slab.getaddbyindex(op->u.data.indexlist[i]);
                        assert(pshm != NULL);
                        copy_length = bufsize > unit_size ? unit_size : bufsize;
                        ::memcpy(tmp_buffer + copy_loc, pshm, copy_length);
                        bufsize -= copy_length;
                        copy_loc += copy_length;
                    }
                    vol->do_create_rbd_completion(op, &comp);
                    vol->do_aio_write(op, off, op->u.data.len, tmp_buffer, comp);						
                }
            } else {
                op->ref_inc();
                pdc_callback(NULL, op);
               //continue;
            }
            break;
        // aio read
        case PDC_AIO_READ:
            /*get share memory units*/
            num_index = pdc->slab.get(op->u.data.len, op->u.data.indexlist);
            if (num_index < 0 ) {
                cerr << "failed to get shm unit,ret is " << num_index << endl;
                //TODO:should we retry to get share memory unit,or just return failed?
                break;
            }
            op->u.data.chunksize = num_index; 
            if (!SERVER_IO_BLACKHOLE) {
                flag = false;
                off = op->u.data.offset;
                bufsize = op->u.data.len;
                //TODO:deal with io_split
                for (int i = 0; i < op->u.data.chunksize; i++) {
                    assert(bufsize > 0);
                    unit_size = pdc->slab.get_unit_size(op->u.data.indexlist[i]);
                    assert(unit_size != -1);
                    char *pdata = 
                        (char *)pdc->slab.getaddbyindex(op->u.data.indexlist[i]);
                    assert(pdata != NULL);
                    read_length = bufsize > unit_size ? unit_size : bufsize;
                    vol->do_create_rbd_completion(op, &comp);
                    vol->do_aio_read(op, off, read_length, pdata, comp);   
                    bufsize -= read_length;
					off += read_length;
				}
            } else {
                op->ref_inc();
                pdc_callback(NULL, op);
            }
            break;
        case PDC_AIO_FLUSH:
            if (!SERVER_IO_BLACKHOLE) {
                if(0){
                    vol->do_create_rbd_completion(op, &comp);
                    vol->do_aio_flush(op, comp);
                }else{
                    op->ref_inc();
                    pdc_callback(NULL, op);
                }
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
        case PDC_AIO_FLUSH:
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

int add_fd(Pdcserver *pdc, Msginfo *op)
{
    int fd;
    CephBackend::RbdVolume *vol;
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
    pdc->OpFindClient( op);
    vol = reinterpret_cast<CephBackend::RbdVolume *>(op->volume);
    p_pipe =reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(vol->mq[RECVMQ]);
    fd = p_pipe->GetFd();
    if(fd> 0){
        pdc->listenlock.lock();

        pdc->fds.push_back(fd);
        pdc->listenlock.unlock();
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
/*
void* Pdcserver::Finisherthreads::_process()
{
    int r;
    int epfd;
    int curfds = 0;
    int fds, n;
    int listenfd;
    u64 sum = 0;
    int threadid;
    struct epoll_event ev;
    struct epoll_event events[EPOLLSIZE];

    Pdcserver *pdc = (Pdcserver *)server;
    CephBackend::RbdVolume *prbd;
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;

    prctl(PR_SET_NAME, "Listenthread");
    cerr<<"listen thread  start"<<endl;
    //if(SHARD_LISTEN){
    r = pdc->msgmq.openpipe();
    if(r< 0){
        cerr<<"listen pipe open failed "<<r<<endl;
        return NULL;
    }
    epfd = ::epoll_create(EPOLLSIZE);  
    if(epfd < 0 ){
        cerr<<"epoll create failed :"<< epfd <<endl;
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
            //if all client use one fifo to write ,then tfd== listenfd ,just do read
            // if every client use it's own fifo fd, then tfd == listenfd ,
            // we need to check if need to add new fd to epoll
            //
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
                    // vm client is down?
                    cerr<<"muliti pipe read buf  is:"<<r<<" but should be:"<<bufsize<<endl;
                    if(tfd != listenfd)
                        del_event(epfd, tfd);
                    else{
                        //all client is shutdown, let cpu free
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

*/

/*SHARD_THREAD for listen tp*/
void* Pdcserver::Finisherthreads::_process()
{
    int r;
    int epfd;
    int curfds = 0;
    int fds, n;
    int listenfd = 0;
    int lfd;
    u64 sum = 0;
    int wait_sum = 0;
    int threadid;
    struct epoll_event ev;
    struct epoll_event events[EPOLLSIZE];

    Pdcserver *pdc = (Pdcserver *)server;
    pdc->listenlock.lock();
    threadid = pdc->listennum++;
    pdc->listenlock.unlock();
    CephBackend::RbdVolume *prbd;
    pdcPipe::PdcPipe<Msginfo>::ptr p_pipe;
    
    prctl(PR_SET_NAME, "Listenthread");

    cerr<<"listen thread  start"<<endl;
    //if(SHARD_LISTEN){
    if(threadid == 0){
    r = pdc->msgmq.openpipe();
    if(r< 0){
        cerr<<"listen pipe open failed "<< r <<endl;
        return NULL;
    }
    epfd = ::epoll_create(EPOLLSIZE);  
    if(epfd < 0 ){
        cerr<<"epoll create failed :"<< epfd <<endl;
        //return NULL;
    }
    listenfd = pdc->msgmq.GetFd();
    add_event(pdc,epfd, ev, NULL, listenfd);
    wait_sum++;
    curfds++;
    }else{
        epfd = ::epoll_create(EPOLLSIZE);  
        if(epfd < 0 ){
            cerr<<"epoll create failed :"<< epfd <<endl;
            //return NULL;
        }
    }
    //}
    while(1){
        if(stop()) continue;
        if(!pdc->fds.empty()){
            lfd = 0;
            
            if(pdc->lastthread == threadid){
                pdc->listenlock.lock();
                lfd = pdc->fds.front();
                pdc->fds.pop_front();
                pdc->lastthread = (pdc->lastthread+1) % pdc->listennum;
                pdc->listenlock.unlock();
            }
            //pdc->listenlock.unlock();

            if(lfd > 0){
                add_event(pdc,epfd, ev, NULL, lfd);
                wait_sum++;
                cerr<<"listentp:" << threadid<< " get  a new fd:"<<lfd<<endl;
            }
        }
        if(wait_sum == 0){
            usleep(100);
            continue;
        }

        fds = epoll_wait (epfd, events, EPOLLSIZE, -1);
        if(fds == -1){
            if(errno == EINTR) continue;
            else {
                cerr<<"epoll wait error:"<<strerror(errno)<<endl;
                break;
            }
        }
        int tfd;
        int bufsize = sizeof(Msginfo);
        for(n = 0; n<fds ;n ++){
            tfd = events[n].data.fd;    //tmp fd
            //if all client use one fifo to write ,then tfd== listenfd ,just do read
            // if every client use it's own fifo fd, then tfd == listenfd ,
            // we need to check if need to add new fd to epoll
            
            Msginfo *op = pdc->msg_pool.malloc();
            r = ::read(tfd, op, bufsize);
            
            if(( tfd == listenfd )&& (events[n].events & EPOLLIN)){  //
                if(op->opcode == PDC_ADD_EPOLL){ // MULTI model to add epoll listen
                    op->dump("pdc add epoll");
                    if(MULTIPIPE){
                       if(SHARD_LISTEN)
                           add_fd(pdc,op);
                       else{
                           add_event(pdc, epfd, ev, op, 0);
                           wait_sum++;
			    }
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
                    if(tfd != listenfd){
                        del_event(epfd, tfd);
                        wait_sum--;
                    }
                    else{
                        /*all client is shutdown, let cpu free*/
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
            client[msg->u.mgr.client.pool] = msg->u.mgr.client.volume;
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
                    //TODO,when we failed to register vm,we shoule send OPEN_RBD_FAILED to client
                    cerr<<"register vm failed ret = "<<r <<endl;
                    break;
                    //delete msg;
                    //continue;
                }
				cerr << "send op OPEN_RBD_FINISH to client" <<endl;
                pdc->OpFindClient(msg);
                vol = reinterpret_cast<CephBackend::RbdVolume *>(msg->volume);
                pipe =reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(vol->mq[SENDMQ]);
                msg->opcode = OPEN_RBD_FINISH;                
                assert(pipe);
                r = pipe->push(msg);
                if (r < 0) {
                    msg->dump("push failed");
                    cerr << "pipe push msg:" << msg->opid << " failed" <<endl;
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
                r = pdc->slab.get(msg->u.data.len, msg->u.data.indexlist);
                if(r <= 0){
                    cerr<<"get memory failed"<<endl;
			//TODO : need more todo
                    //break;
                }
                msg->u.data.chunksize = r;
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
                    cerr<<"shm->put falied:"<<n <<" and index is:"<<msg->u.data.chunksize<<endl;
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

    listennum = 0;
    lastthread = 0;
    listen = new Finisherthreads("Listen threadpool", this);
    listen->setname("Listen threadpool");
    listen->init(2);	

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
    string pool(op->u.mgr.client.pool);
    string volume(op->u.mgr.client.volume);
    //map<string,string> opclient;
    //opclient[pool] = volume;
    string backendname("ceph");
    CephBackend *backend = clusters[backendname];
    if(!backend){
        cerr<<"find no backend now for:"<<backendname<<endl;
        assert(0);
        return;
    }
    //op->volume =  backend->findclient( &opclient);
    op->volume = backend->findclientnew(pool, volume);
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

    server->wait_to_shutdown();
    //todo : shutdown
    return 0;
}

