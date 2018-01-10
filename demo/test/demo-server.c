#include <stddef.h>
#include <iostream>
//#include <ringq.hpp>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <iostream>
#include <sys/time.h>
#include "ringq.hpp"
#include "pipe.hpp"

using namespace std;
using namespace wp::shmQueue;
using namespace wp::Pipe;
struct Msinfo{
    int stat;
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

int main()
{
    int i = 0;
    int sum = 10000;
    Msinfo *ack;
    Msinfo *ms = new Msinfo();
    struct timeval start,end;

   sum = 10000;
   i = 0;
   Pipe<Msinfo> pipemq("123",12, PIPEWRITE,  PIPECLIENT);
   Pipe<Msinfo> pipeack("456",45, PIPEWRITE,  PIPESERVER);

    //if(pipemq.Init() < 0 )  return  -1;
    //if(pipeack.Init() < 0 )  return  -1;
    pipemq.Init();
    pipeack.Init();
   memset(ms, 222,sizeof(Msinfo));
   gettimeofday(&start,NULL);
   while(sum --){
   	int r = pipemq.push(ms);
       if( r <= 0){
           cerr <<" pipe push ms failed"<<r<<endl;
	    //sleep(1);
	    continue;
       }
       //if(pipeack.isEmpty()){
       if(1){
           ack = pipeack.pop();
           if(ack)
           delete ack;
	
       }
       i ++;
   }
   gettimeofday(&end,NULL);
   
   cerr <<"using PIPE: =================="<<endl;
   cerr<< "communication :["<< i<<"] times , use time:"<<timediff(&start,&end) <<endl;

}
