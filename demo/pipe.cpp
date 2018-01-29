//g++ msserver.c -o server
//g++ msclient.c -o client
#include "type.h"
#include <iostream>
#include <sstream>

#include "pipe.hpp"

using namespace std;
using namespace pdcPipe;
int atokey = 251;
int GetMqKey() { cerr<<"Getkey:"<<atokey<<endl;return atokey++; }
namespace pdcPipe{

/*
client pipe with server pipe mapping info:
client:	sendmq		recvmq
server:	recvmq		sendmq

*/
void copymqs(map<string ,void *> &mqs,PdcPipe<Msginfo>* send, PdcPipe<Msginfo>*recv)
{
    cerr<<"RBD USE RADOS'S pipes mq"<<endl;
    assert(send);
    assert(recv);
    mqs.insert(pair<string,void *>("recv", (void*)recv));
    if(send)
    mqs.insert(pair<string,void *>("send", (void*)send));


}
int createpublicqueues(map<string ,void *> &mqs,bool sw)
{
    int r;
    
    if(sw){   //sw = true means: client
        pdcPipe::PdcPipe<Msginfo>::ptr sendmq = new pdcPipe::PdcPipe<Msginfo>(PIPEKEY,PIPESEMKEY,PIPEWRITE ,PIPECLIENT);
        r = sendmq->Init();
        if(r < 0){
            cerr<<"create recvmq failed:"<< r <<endl;
        }
        mqs.insert(pair<string,void *>("send", (void*)sendmq));
    }

    return r;
}
int createclientqueues(map<string ,void *> &mqs,bool sw)
{
    stringstream newkey, newsendkey;
    string pkey, pskey;
    int skey;
    int r;
    pid_t pid= getpid();
    newkey<<"/tmp/"<<pid<<GetMqKey();
    newkey >>pkey;
    skey = GetMqKey() +pid;
    
    cerr<<"create client queue:"<<pkey<<" semkey:"<<skey<<" sw is:"<<sw<<endl;
    pdcPipe::PdcPipe<Msginfo>::ptr recvmq = new pdcPipe::PdcPipe<Msginfo>(PIPESERVER);
    recvmq->ResetPipeKey(pkey);
    recvmq->ResetSemKey(skey);
    r = recvmq->Init();
    if(r < 0){
        cerr<<"create recvmq failed:"<<r<<endl;
    }
    
    if(MULTIPIPE){
        
        newsendkey<<"/tmp/"<<pid<<GetMqKey();
        newsendkey >>pskey;
        skey = GetMqKey() +pid;
        cerr<<"create client send queue:"<<pskey<<" semkey:"<<skey<<" sw is:"<<sw<<endl;
        //pdcPipe::PdcPipe<Msginfo>::ptr sendmq = new pdcPipe::PdcPipe<Msginfo>(PIPEKEY,PIPESEMKEY,PIPEWRITE ,PIPECLIENT);
        pdcPipe::PdcPipe<Msginfo>::ptr sendmq = new pdcPipe::PdcPipe<Msginfo>(PIPECLIENT);
        sendmq->ResetPipeKey(pskey);
        sendmq->ResetSemKey(skey);
        r = sendmq->Init();
        if(r < 0){
            cerr<<"create sendmq failed:"<< r <<endl;
        }
        mqs.insert(pair<string,void *>("send", (void*)sendmq));
    }

    mqs.insert(pair<string,void *>("recv", (void*)recvmq));


    return 0;
}



/* for client is send mq, for server is recv mq. msg is client's info,
*   so we need to create reserved 
*
*/
int createserverqueues(void * mqkeys,map<string ,void *> &mqs)
{

    int r;
    pkey * pk = (pkey*)mqkeys;
    string newkey(pk->key);

    cerr<<"create server queues: "<<newkey <<" sem:"<<pk->semkey<<endl;
    if(1){
        
        pdcPipe::PdcPipe<Msginfo>::ptr sendmq = new pdcPipe::PdcPipe<Msginfo>(PIPECLIENT);
        sendmq->ResetPipeKey(newkey);
        sendmq->ResetSemKey(pk->semkey);
        r = sendmq->Init();
        if(r < 0){
            cerr<<"create send failed:"<< r <<endl;
            return -1;
        }
        
        r = sendmq->openpipe();
        mqs.insert(pair<string,void *>(SENDMQ, (void*)sendmq));
    }

    if(1){
        // SERVER's 
        
        pdcPipe::PdcPipe<Msginfo>::ptr recvmq = new pdcPipe::PdcPipe<Msginfo>(PIPESERVER);
        recvmq->ResetPipeKey(pk->recvkey);
        recvmq->ResetSemKey(pk->recvsem);
        r = recvmq->Init();
        if(r < 0){
            cerr<<"create recvmq failed:"<<r<<endl;
            return -1;
        }
        r = recvmq->openpipe();
        mqs.insert(pair<string,void *>(RECVMQ, (void*)recvmq));

    }
	
    return 0;
}

int updateserverqueues(void * mqkeys,map<string ,void *> &mqs)
{
    int r;
    pdcPipe::PdcPipe<Msginfo>::ptr mq;
    map<string , void *>::iterator it;
    
    for(it = mqs.begin(); it != mqs.end();it++){
        cerr<<"delete pipe :"<<it->first<<endl;
        mq = reinterpret_cast<pdcPipe::PdcPipe<Msginfo>::ptr>(it->second);
        delete mq;
    }
    mqs.clear();

    r = createserverqueues(mqkeys, mqs);
    return r;
}


    
}

