//g++ msserver.c -o server
//g++ msclient.c -o client
#include "pdc_lock.hpp"
#include <iostream>
#include <sstream>


PdcLock::PdcLock(string nm):
    name(nm),locked(false)
{
     
    pthread_mutex_init(&_mutex,NULL);
    
}
PdcLock::PdcLock( ):
    name("lock"),locked(false)
{
    pthread_mutex_init(&_mutex,NULL);

}

void PdcLock::lock()
{
    pthread_mutex_lock(&_mutex);
    locked = true;
}
void PdcLock::unlock()
{
   
    pthread_mutex_unlock(&_mutex);
    locked = false;
}





