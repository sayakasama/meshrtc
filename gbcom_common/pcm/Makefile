root_path = ../..
common_path = $(root_path)/common

third_party = /home/test/webrtc/other/release

include_path = -I/usr/local/include -I$(common_path) -I$(common_path)/h264 -I$(common_path)/pcm \
				-I$(common_path)/json -I$(common_path)/comm  -I$(common_path)/comm/ipc -I$(common_path)/websocket -I$(common_path)/queue -I$(common_path)/mesh_call

obj-app-c = pcm_test 

CFLAGS  =  -Wall -g -DPCM_SELF_TEST $(include_path)
LDLIBS +=  --static -lm -lpthread -L$(third_party)/lib
LDFLAGS +=  

MYSRC    = ${shell pwd}

BIN_SRC=$(shell echo $(MYSRC)/*.c)

OBJS_pcm  = $(common_path)/pcm/pcm.o  $(common_path)/pcm/resample.o

OBJS_c =  $(OBJS_pcm) 

build: $(obj-app-c) 

$(obj-app-c):$(OBJS_c)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) 
	cp $(obj-app-c) $(root_path)/bin
	
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

install:
	@echo "INSTALL       $(obj-app-c)"

clean:
	@echo "CLEAN         $(obj-app-c) "
	@rm -rf $(OBJS_c)
	@rm -rf $(obj-app-c) 
