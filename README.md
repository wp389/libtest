for just me test.   
2018:1-23   
fio  bs=4k size=10G iodepth=128   iops=12526/10ms ~~14837/8602us   
fio for blackhole test is 133k iops with 206us   

2018:1-25   
shmmemory change  init  method    
suport multi client to run ,
fio with blockhole is 160k iops.

2018:1-26
multiclient can run to 200k iops with 1ms latency
increase some multipipe function but not enabled

2018:1-29
multclient can run to 240k iops + 1.3ms~ 1.5ms latency \    
in blackhole,with multipipe model    



