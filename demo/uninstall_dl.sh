#!/bin/sh
echo "remove soft link and libpdc"
rm /lib64/librbd.so.1 /lib64/librados.so.2 -f
rm /lib64/pdc/librbd.so.1 /lib64/pdc/librados.so.2 -f
rm /lib64/libpdc.so.1.0.0 -f

if [ -d /lib64/pdc ];then
    if [ -f /lib64/pdc/librbd.so.1.0.0 ];then
        mv /lib64/pdc/librbd.so.1.0.0 /lib64/
    fi
    if [ -f /lib64/pdc/librados.so.2.0.0 ];then
        mv /lib64/pdc/librados.so.2.0.0 /lib64/
    fi
	rm -fr /lib64/pdc
fi
echo "link librbd.so and librados.so to librbd and librados"
ln -s /lib64/librbd.so.1.0.0 /lib64/librbd.so.1
ln -s /lib64/librados.so.2.0.0 /lib64/librados.so.2
