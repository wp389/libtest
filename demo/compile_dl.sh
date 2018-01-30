#!/bin/sh
if [ -d obj/ ];then
	rm obj/* -f
else
	mkdir obj
fi
g++ pdcserver.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -std=c++11 -o obj/server -lpthread -Wl,-rpath=/lib64/pdc/ -lrbd -lrados -lboost_system  2>obj/server.log
g++ -shared -fPIC libpdc.cc pdcclient.cpp backend_client.cpp pipe.cpp type.cc pdc_lock.cpp -g -std=c++11 -o obj/libpdc.so.1.0.0 -lpthread -lboost_system
gcc test/test.c -o obj/vmtest -lrbd -lrados
