include ../makefile.inc.lv1
SHARE_INC_PATH=./include/

all:json_value.o json_writer.o json_reader.o
	@cp -r ./include/* $(TARGET_INC_PATH) ; cp *.o $(TARGET_LIB_PATH)

%.o:./src/%.cpp 
	$(DC)

%.o:./src/%.cc 
	$(DC)

%.o:./src/%.c 
	$(DC)

%.o:./src/%.cpp ./include/%.h
	$(DC)

%.o:./src/%.cc ./include/%.h
	$(DC)

%.o:./src/%.c ./include/%.h
	$(DC)


clean:
	@rm -rf  *.o
	@rm -rf $(all)
