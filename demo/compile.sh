rm obj/* -f
g++ pdcserver.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/server -lpthread  2>obj/server.log

#g++ libpdc.cpp pdcclient.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/client -lpthread  2>obj/client.log
g++ -shared -fPIC libpdc.cc pdcclient.cpp backend_client.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/libpdc.so.1.0.0 -lpthread

#g++ pdcserver.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/server -lpthread -L/lib64 -lrbd -lrados 2>obj/server.log
#g++ libpdc.cpp pdcclient.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/client -lpthread -L/lib64 -lrbd -lrados  2>obj/client.log

ls obj/



#g++ test.c -o pdctest -Wl,-rpath=/home/w/code/test/demo/obj/ -L/home/w/code/test/demo/obj/ -lrbd
