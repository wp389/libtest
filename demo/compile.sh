#!/bin/sh
rm obj/* -f
g++ pdcserver.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -std=c++11 -o obj/server -lpthread -Wl,-rpath=/lib64/pdc/ -lrbd -lrados -lboost_system  2>obj/server.log

#g++ libpdc.cpp pdcclient.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/client -lpthread  2>obj/client.log
g++ -shared -fPIC libpdc.cc pdcclient.cpp backend_client.cpp pipe.cpp type.cc pdc_lock.cpp -g -std=c++11 -o obj/libpdc.so.1.0.0 -lpthread -lboost_system

#g++ pdcserver.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/server -lpthread -L/lib64 -lrbd -lrados 2>obj/server.log
#g++ libpdc.cpp pdcclient.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/client -lpthread -L/lib64 -lrbd -lrados  2>obj/client.log

gcc test/test.c -o obj/vmtest -lrbd -lrados
#g++ test.c -o pdctest -Wl,-rpath=/home/w/code/test/demo/obj/ -L/home/w/code/test/demo/obj/ -lrbd
