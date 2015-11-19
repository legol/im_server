include ../makefile.inc.lv1

all:atomic.o thread_model.o timer.o general_data.o thread_pool.o mem_pool.o object_pool.o misc.o
	@cp *.h $(TARGET_INC_PATH) ; cp *.o $(TARGET_LIB_PATH)

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
