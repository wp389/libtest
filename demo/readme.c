/*this is for dftdc  test
   dc will convert all data to center

*/
for mstest:

g++ msserver.c -o server
g++ msclient.c -o client


for client server test:

g++ pdcserver.cpp backend_ceph.cpp -o obj/server -L/lib64/ -lpthread -lrbd -lrados 




