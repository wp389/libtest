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
                vector<u64> index(op->data.indexlist,op->data.indexlist+op->data.chunksize);
                n = pdc->slab.put(index);
                if(n < 0 ){
                    cerr<<"shm->put falied:"<<n <<" and index is:"<<index.size()<<endl;
                    //return ;
                    
                }
            }
            
        }
        pdcPipe::PdcPipe<Msginfo>*p_pipe = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>*>(prbd->mq[SENDMQ]);
        r = p_pipe->push(op);
        if(r < 0){
            cerr<<"callback to push pipe error:"<<r<<endl;
        }
        //TO DELETE OP
        
    }
    delete op;
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
        
        pdc->iolock.lock();

        //pdc->iocond.wait(pdc->iolock);
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
        switch(op->opcode){
        case PDC_AIO_WRITE:
        if(!SERVER_IO_BLACKHOLE){    //black hole
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
            lengh = bufsize > CHUNKSIZE ? CHUNKSIZE:bufsize;
            vol->do_aio_write(op, off+ i*CHUNKSIZE, lengh, (char *)pdata, comp);   
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

        if(op)
            op->dump("listen thread get op");
        switch(op->opcode){
        case PDC_AIO_WRITE:
            //op->opcode == PDC_AIO_WRITE;
            pdc->iolock.lock();
            pdc->ops.push_back(op);
            //pdc->iocond.Signal();
            pdc->iolock.unlock();
            
            break;
        case GET_MEMORY:
            pdc->msglock.lock();
            pdc->msgop.push_back(op);
            //pdc->msgcond.Signal();
            pdc->msglock.unlock();
            	
            break;
        default:  //mgr cmd
            pdc->msglock.lock();
            pdc->msgop.push_back(op);
            //pdc->msgcond.Signal();
            pdc->msglock.unlock();
            
            break;
        }
        sum++;



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
                cerr<<"add_event fd is "<<ev.data.fd<<" maybe not opened"<<endl;
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
            //if(MULTIPIPE)
            Msginfo *op = new Msginfo();
            r = ::read(tfd, op, bufsize);
            
            if(( tfd == listenfd )&& (events[n].events & EPOLLIN)){  //
                if(op->opcode == PDC_ADD_EPOLL){ // MULTI model to add epoll listen
                    op->("pdc add epoll");
                    if(MULTIPIPE)
                       add_event(pdc, epfd, ev, op, 0);
                       curfds++;
                       //ev.data.fd = ;
                    delete op;
                }else{
                  if(r == bufsize){
                       r = handle_listen_events(server,op);
                  }else{
                       cerr<<"pipe read buf  is:"<<r<<" but should be:"<<bufsize<<endl;
                  }
                }
            }else{
                if(r == bufsize){
                    r = handle_listen_events(server,op);
                }else{
                        // need  delete ?
                        cerr<<"pipe read buf  is:"<<r<<" but should be:"<<bufsize<<endl;
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
    u64 sum = 0;
    CephBackend::RbdVolume *vol;
    pdcPipe::PdcPipe<Msginfo>::ptr pipe;

    Pdcserver *pdc = (Pdcserver *)server;
    cerr<<"MSGthread "<<pthread_self()<<" start"<<endl;

    //pdc->msglock.lock();
    while(1){
        if(stop() ) continue;
        pdc->msglock.lock();
        
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
                    continue;
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
                if(r <= 0){
                    cerr<<"get memory failed"<<endl;
                    continue;
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
                    continue;
                }
                break;
            }
            default:
            {
                cerr<<"opcode error:"<<msg->opcode<<endl;
                assert(0);
                break;
            }

         }
        }
        delete msg;
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
    //pthread_cond_init(&iocond);
    //pthread_mutex_init(&iomutex,NULL);
    iothread = new Pdcserver::Iothreads("IO-threadpool", this);
    iothread->init(1);

    //pthread_cond_init(&msgcond);
    //pthread_mutex_init(&msgmutex,NULL);
    msgthread = new Msgthreads("MSG-threadpool",this);
    msgthread->init(2);

    //pthread_cond_init(&finicond);
    //pthread_mutex_init(&finimutex,NULL);
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

