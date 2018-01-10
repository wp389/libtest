CC=g++
PATH=./obj/
SRCS=pdcserver.cpp\
	backend_ceph.cpp\
	pipe.cpp\
	type.cc
LDFLAGS	:= -lpthread -g
OBJS=$(SRCS:.CPP=.0)
EXEC=$(PATH)server
start:$(OBJS)
	$(CC) -o $(EXEC) $(OBJS) $(LDFLAGS)
.cpp.o:
	$(CC) -o $(PATH)$@ -c $< $(LDFLAGS)

clean:
	rm -rf $(PATH)$(OBJS)

