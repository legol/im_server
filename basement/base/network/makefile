include ../makefile.inc.lv1

# 2.4 的内核需要自己编译 epoll.c
all:networkutil.o epollevent.o tcppeer.o tcpserver.o tcpclient.o timeoutmgr.o tcppacket.o epoll_thread.o server_thread.o client_thread.o 
	@cp *.h $(TARGET_INC_PATH) ; cp *.o $(TARGET_LIB_PATH)

#epoll.o:epoll.c
#	cc -g -Wall -o epoll.o -c epoll.c 

%.o:%.cpp 
	$(DC)

%.o:%.cc 
	$(DC)

%.o:%.c 
	$(DC)

%.o:%.cpp %.h
	$(DC)

%.o:%.cc %.h
	$(DC)

%.o:%.c %.h
	$(DC)


clean:
	@rm -rf  *.o
	@rm -rf $(all)
