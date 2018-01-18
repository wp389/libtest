rm obj/* -f
g++ pdcserver.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/server -lpthread -Wl,-rpath=/pdc/lib64/ -L/pdc/lib64/ -lrbd -lrados  2>obj/server.log

#g++ libpdc.cpp pdcclient.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/client -lpthread  2>obj/client.log
g++ -shared -fPIC libpdc.cc pdcclient.cpp backend_client.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/libpdc.so.1.0.0 -lpthread

#g++ pdcserver.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/server -lpthread -L/lib64 -lrbd -lrados 2>obj/server.log
#g++ libpdc.cpp pdcclient.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -o obj/client -lpthread -L/lib64 -lrbd -lrados  2>obj/client.log

ls obj/
rm /lib64/librbd.so -f
rm /lib64/librados.so -f
cp -f obj/libpdc.so.1.0.0 /lib64/
echo "to change librbd.so"
ln -s /lib64/libpdc.so.1.0.0 /lib64/librbd.so
ln -s /lib64/libpdc.so.1.0,0 /lib64/librados.so

gcc test/test.c -o obj/vmtest -lrbd -lrados
#g++ test.c -o pdctest -Wl,-rpath=/home/w/code/test/demo/obj/ -L/home/w/code/test/demo/obj/ -lrbd
