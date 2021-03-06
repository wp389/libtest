echo "start to compile"
rm obj/server obj/libpdc* obj/vmtest  -f

g++ pdcserver.cpp backend_ceph.cpp pipe.cpp type.cc pdc_lock.cpp -g -std=c++11 -o obj/server -lpthread  -lboost_system -L/lib64/ -lrbd -lrados  #2>obj/server.log

g++ -shared -fPIC libpdc.cc pdcclient.cpp backend_client.cpp pipe.cpp type.cc pdc_lock.cpp -g -std=c++11 -o obj/libpdc.so.1.0.0 -lpthread -lboost_system
gcc test/test.c -o obj/vmtest  -Wl,-rpath=/pdc/lib64/ -L/pdc/lib64/ -lrbd -lrados

ls obj/

exit 0
ls obj/
rm /pdc/lib64/libpdc.so.1.0.0 -f
cp  obj/libpdc.so.1.0.0 /pdc/lib64/

rm /pdc/lib64/librbd.so -f
rm /pdc/lib64/librados.so -f
rm /pdc/lib64/librbd.so.1 -f
rm /pdc/lib64/librados.so.2 -f

echo "to change librbd.so"
ln -s /pdc/lib64/libpdc.so.1.0.0  /pdc/lib64/librbd.so
ln -s /pdc/lib64/libpdc.so.1.0.0  /pdc/lib64/librados.so
ln -s /pdc/lib64/libpdc.so.1.0.0  /pdc/lib64/librbd.so.1
ln -s /pdc/lib64/libpdc.so.1.0.0  /pdc/lib64/librados.so.2

echo "compile test:"
gcc test/test.c -o obj/vmtest  -Wl,-rpath=/pdc/lib64/ -L/pdc/lib64/ -lrbd -lrados
