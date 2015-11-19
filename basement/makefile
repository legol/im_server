#include ../public/Makefile.def
MODULE_NAME = basement

BINDIR=./output
TARGET=test/test 
TARGET1=test/test_json
INCLUDE_PATH=./include
LIBRARY_PATH=./lib
BASE=base

INC=-I$(INCLUDE_PATH) -I./
LIB=-L$(LIBRARY_PATH) -lrt -lcrypto -ldl -lz $(LIBRARY_PATH)/*.o -lboost_thread -lboost_regex -lboost_system -lpthread

#DC=g++ -g -Wall -DBUILD_TIME="$(BUILD_TIME)"  -DPROJECT_VERSION="$(PROJECT_BUILD_VERSION)" -DMODULE_VERSION="$(MODULE_BUILD_VERSION)" -DTAG_VERSION="$(TAG_LAST)" -o $@ -c $<
#DT=g++ -g -Wall -DBUILD_TIME="$(BUILD_TIME)"  -DPROJECT_VERSION="$(PROJECT_BUILD_VERSION)" -DMODULE_VERSION="$(MODULE_BUILD_VERSION)" -DTAG_VERSION="$(TAG_LAST)" -o $@ $^

DC=g++ -g -Wall -o $@ -c $<
DT=g++ -g -Wall -o $@ $^

AR = ar r
RANLIB = ranlib

release:sub_dir static_lib clrtarget $(TARGET) $(TARGET1)
	cp test/test $(BINDIR)
	cp test/test_json test/test_json.json $(BINDIR)

sub_dir: dir
#   -@mkdir include
#   -@mkdir lib
	@for d in $(BASE) ; do \
	make -C $$d ; \
	echo "" ;\
	done

dir:
	@if [ ! -d $(INCLUDE_PATH) ]; then mkdir $(INCLUDE_PATH); fi
	@if [ ! -d $(LIBRARY_PATH) ]; then mkdir $(LIBRARY_PATH); fi

static_lib: 
	$(AR) ./lib/lib$(MODULE_NAME).a ./lib/*.o
	$(RANLIB) ./lib/lib$(MODULE_NAME).a

clrtarget:
	-@rm -rf $(TARGET)
	-@rm -rf $(TARGET1)

OBJS_TEST = test/main.o test/handler.o test/client_cb.o
test/test:$(OBJS_TEST) 
	$(DT) $(LIB) 
	@echo ""


OBJS_TEST_JSON = test/test_json.o
test/test_json:$(OBJS_TEST_JSON)
	$(DT) $(LIB)
	@echo ""

%.o:%.cpp 
	$(DC) $(INC)

%.o:%.cc 
	$(DC) $(INC)

%.o:%.c 
	$(DC) $(INC)

%.o:%.cpp %.h
	$(DC) $(INC)

%.o:%.cc %.h
	$(DC) $(INC)

%.o:%.c %.h
	$(DC) $(INC)

clean: dir
	@for d in $(BASE) ; do \
	make clean -C $$d ; \
	echo "" ;\
	done
	-@rm -rf $(INCLUDE_PATH)/*.h
	-@rm -rf $(INCLUDE_PATH)/json
	-@rm -rf $(LIBRARY_PATH)/*.o $(LIBRARY_PATH)/*.a
	-@rm -rf  *.o
	-@rm -rf  test/*.o
	-@rm -rf $(TARGET)
	-@rm -rf output/*


