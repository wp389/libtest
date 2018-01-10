#include <stddef.h>
#include <iostream>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <iostream>
#include <stdlib.h>
#include "ringq.hpp"
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#include "pipe.hpp"
using namespace std;
using namespace wp::shmQueue;
using namespace wp::Pipe;

struct Msinfo{
    int state;
    char info[32];
    Msinfo() {}
};
double timediff(struct timeval* s, struct timeval* e)
{
  double diff;
  time_t ss;
  suseconds_t u;

  ss = e->tv_sec - s->tv_sec;
  u =  e->tv_usec - s->tv_usec;

  diff = ss;
  diff *= 1000000.0;
  diff += u;

  return diff;


}

struct clientconf{
    int threadnum;
    pthread_t thread_id;

};

int main()
{
    int fd = -1;
    
    int i = 0;
    int sum = 10000;
    Msinfo *ms ;
    Msinfo *send = new Msinfo();
    struct timeval start,end;

    Pipe<Msinfo> pipemq("123", 12, PIPEWRITE,  PIPESERVER);
    Pipe<Msinfo> pipeack("456", 45, PIPEWRITE,  PIPECLIENT);
	
    cerr << "client start"<<"start PIPE server"<<getpid()<<endl;
 
    //if(pipemq.Init() < 0 )	return	-1;
    //if(pipeack.Init() < 0 )  return  -1;
    pipemq.Init();
    pipeack.Init()
    unsigned long long  flag = 0;
    //gettimeofday(&start,NULL);
    while(1){
        ms = pipemq.pop();
        if(!ms){
	     cerr <<"pop ms failed"<<endl;
            //sleep(1);
	     continue;
        }
        flag++;
        gettimeofday(&start,NULL);
        delete ms;
        pipeack.send(send);
        if(flag >= 10000 ){
            gettimeofday(&end,NULL);
            cerr <<"server PIPE queue ack:"<<flag<<" times	use time:"<<timediff(&start, &end )
		 	<< " us or:"<<(timediff(&start, &end ) /1000000)<<" s"<<endl;
             flag = 0;
         }
    }
			 
	
}
