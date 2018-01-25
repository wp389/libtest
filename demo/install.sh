
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
