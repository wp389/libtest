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
int createclientqueues(map<string ,void *> &mqs,bool sw)
{
    stringstream newkey;
    string pkey;
    int skey;
    int r;
    
    newkey<<GetMqKey();
    newkey >>pkey;
    skey = GetMqKey();

    pdcPipe::PdcPipe<Msginfo>::ptr recvmq = new pdcPipe::PdcPipe<Msginfo>(PIPESERVER);
    recvmq->ResetPipeKey(pkey);
    recvmq->ResetSemKey(skey);
    r = recvmq->Init();
    if(r < 0){
        cerr<<"create recvmq failed:"<<r<<endl;
    }
    if(!sw){
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
int createserverqueues(map<string,string>&pipekeys, map<string,int>semkeys,map<string ,void *> &mqs)
{

    int r;
    map<string ,string >::iterator pit = pipekeys.find("send");
    map<string ,int >::iterator sit = semkeys.find("send");
	
    assert(pit != pipekeys.end() || sit!=semkeys.end());

    cerr<<"create server queues: "<<pit->second <<" sem:"<<sit->second<<endl;

    pdcPipe::PdcPipe<Msginfo>::ptr sendmq = new pdcPipe::PdcPipe<Msginfo>(PIPECLIENT);
    sendmq->ResetPipeKey(pit->second);
    sendmq->ResetSemKey(sit->second);
    r = sendmq->Init();
    if(r < 0){
        cerr<<"create recvmq failed:"<<r<<endl;
        return -1;
    }
	
    mqs.insert(pair<string,void *>("send", (void*)sendmq));
    
    return 0;
}

    
}

