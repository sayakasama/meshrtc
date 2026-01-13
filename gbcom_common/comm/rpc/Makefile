obj-app-c = client 
obj-app-s = server

root_path = ../../../
common_path = $(root_path)/common
third_party = /home/test/webrtc/other/release

include_path = -I$(common_path) -I$(common_path)/comm -I$(third_party)/include

CFLAGS  =  -Wall -g $(include_path)
LDLIBS +=  -L$(third_party)/lib -lgsoap
LDFLAGS +=  --static 

MYSRC    = ${shell pwd}

BIN_SRC=$(shell echo $(MYSRC)/*.c)

OBJS_c = client.o soapClient.o soapC.o stdsoap2.o
OBJS_s = server.o soapServer.o soapC.o stdsoap2.o

build: $(obj-app-c) $(obj-app-s)

$(obj-app-c):$(OBJS_c)
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $^

$(obj-app-s):$(OBJS_s)
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $^
	
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

install:
	@echo "INSTALL       $(obj-app-c)"

clean:
	@echo "CLEAN         $(obj-app-c) $(obj-app-s) "
	@rm -rf *.o 
	@rm -rf $(obj-app-c) $(obj-app-s)  
