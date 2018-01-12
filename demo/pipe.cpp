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


void copymqs(map<string ,void *> &mqs,PdcPipe<Msginfo>* send, PdcPipe<Msginfo>*recv)
{
    cerr<<"RBD USE RADOS'S pipes mq"<<endl;
    mqs.insert(pair<string,void *>("recv", (void*)recv));
    mqs.insert(pair<string,void *>("recv", (void*)send));


}
int createclientqueues(map<string ,void *> &mqs,bool sw)
{
    stringstream newkey;
    string pkey;
    int skey;
    int r;

    newkey<<GetMqKey();
    newkey >>pkey;
    skey = GetMqKey();

    cerr<<"create client queue:"<<pkey<<" sw is:"<<sw<<endl;
    pdcPipe::PdcPipe<Msginfo>::ptr recvmq = new pdcPipe::PdcPipe<Msginfo>(PIPESERVER);
    recvmq->ResetPipeKey(pkey);
    recvmq->ResetSemKey(skey);
    r = recvmq->Init();
    if(r < 0){
        cerr<<"create recvmq failed:"<<r<<endl;
    }
    //if(!sw)
    if(1){
        pdcPipe::PdcPipe<Msginfo>::ptr sendmq = new pdcPipe::PdcPipe<Msginfo>(PIPEKEY,PIPESEMKEY,PIPEWRITE ,PIPECLIENT);
        r = sendmq->Init();
        if(r < 0){
            cerr<<"create recvmq failed:"<<r<<endl;
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
            cerr<<"create recvmq failed:"<<r<<endl;
            return -1;
        }
    
        mqs.insert(pair<string,void *>(SENDMQ, (void*)sendmq));
    }
	
    return 0;
}

    
}

