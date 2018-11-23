CC=gcc
CCX=g++
INCS = $(shell pkg-config --cflags glib-2.0 json-glib-1.0) -I. -I$(shell pg_config --includedir) -I/usr/local/pgsql/include
LIBS =  $(shell pkg-config --libs glib-2.0 json-glib-1.0)  -L$(shell pg_config --libdir)   -Lc:/msys32/usr/local/pgsql/lib -lpq -lcurl #-lws2_32

CFLAGS=$(INCS) -c -Wall -fpermissive
LDFLAGS=$(LIBS) -s -O3

CSOURCES=\
	package_locale.c \
	lib_net/sock_func.c \
	lib_net/net_func.c \
	lib_net/server_thread.c \
	lib_net/websocket_func.c \
	lib_postgresql/lib_postgresql.c \
	my_time.c \

CPPSOURCES=\
	daemon/daemon.cpp \
	daemon/log.cpp \
	daemon/monitor.cpp \
	daemon/work.cpp \
	lib_protocol/crc16.cpp \
	lib_protocol/protocol.cpp \
	lib_protocol/protocol_parse.cpp \
	lib_protocol/protocol_parse_wialon_ips.cpp \
	lib_protocol/protocol_wialon_ips.cpp \
	lib_protocol/protocol_parse_osmand.cpp \
	lib_protocol/protocol_osmand.cpp \
	lib_protocol/protocol_parse_gt06.cpp \
	lib_protocol/protocol_gt06.cpp \
	lib_protocol/protocol_parse_babywatch.cpp \
	lib_protocol/protocol_babywatch.cpp \
	base_dinamic.cpp \
	base_lib.cpp \
	encode.cpp \
	gps_server.cpp \
	main.cpp \
	md5.cpp \
	server_api.cpp \
	server_thread_api.cpp \
	server_thread_dev.cpp \
	server_thread_http.cpp \
	zlib_util.cpp


OBJECTS=$(CPPSOURCES:.cpp=.o) $(CSOURCES:.c=.o)
EXECUTABLE=gps_server

all: $(CSOURCES) $(CPPSOURCES) $(EXECUTABLE)

OUTPUTDIR = ./bin/
MKDIR = mkdir -p $(OUTPUTDIR)
	
$(EXECUTABLE): $(OBJECTS) 
	$(MKDIR)
	$(CCX) $(OBJECTS) $(LDFLAGS) -o $(OUTPUTDIR)$@
	$(RM) ./*.o
	$(RM) ./lib_net/*.o
	$(RM) ./lib_postgresql/*.o
	$(RM) ./lib_protocol/*.o
	$(RM) ./daemon/*.o

.cpp.o:
	$(CCX) $(CFLAGS) $< -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@


.PHONY: clean
clean:
	$(RM) -rf $(OUTPUTDIR)
	$(RM) ./*.gc??
	$(RM) ./*.o ./lib_net/*.o ./lib_postgresql/*.o ./lib_protocol/*.o ./daemon/*.o
