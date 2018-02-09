#!/bin/sh

echo "remove original soft link"
rm /lib64/librbd.so.1 /lib64/librados.so.2 -f
echo "copy libpdc"
rm -f /lib64/libpdc.so.1.0.0
cp obj/libpdc.so.1.0.0 /lib64/
echo "link librbd.so and librados.so to libpdc"
ln -s /lib64/libpdc.so.1.0.0 /lib64/librbd.so.1
ln -s /lib64/libpdc.so.1.0.0 /lib64/librados.so.2
echo "copy librbd and librados to new directory"
if [ ! -d /lib64/pdc ];then
    mkdir /lib64/pdc
fi
if [ -f /lib64/librbd.so.1.0.0 ];then
    mv /lib64/librbd.so.1.0.0 /lib64/pdc/
    ln -s /lib64/pdc/librbd.so.1.0.0 /lib64/pdc/librbd.so.1
fi
if [ -f /lib64/librados.so.2.0.0 ];then
    mv /lib64/librados.so.2.0.0 /lib64/pdc/
    ln -s /lib64/pdc/librados.so.2.0.0 /lib64/pdc/librados.so.2
fi
