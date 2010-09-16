
#--------------------------------------------------------------------

CC = gcc
AR = ar cru
CFLAGS = -Wall -D_REENTRANT -D_GNU_SOURCE -g -fPIC
SOFLAGS = -shared
LDFLAGS = -lstdc++ -lpthread -lresolv

LINKER = $(CC)
LINT = lint -c
RM = /bin/rm -f

OPENSSL_INCL = -I/usr/include/openssl/
OPENSSL_LIB  = -L/usr/lib -lssl -lcrypto

LIBEVENT_LIB  = -L$(HOME)/libevent -levent

SPSERVER_INCL = -I../spserver
SPSERVER_LIB  = -L../spserver -lspserver

CFLAGS  += $(OPENSSL_INCL) $(SPSERVER_INCL)
LDFLAGS += $(LIBEVENT_LIB) $(OPENSSL_LIB) $(SPSERVER_LIB)

#--------------------------------------------------------------------

LIBOBJS = spopenssl.o

TARGET =  libspopenssl.so \
	testechos testhttps

#--------------------------------------------------------------------

all: $(TARGET)

libspopenssl.so: $(LIBOBJS)
	$(LINKER) $(SOFLAGS) $^ -o $@

testechos: testechos.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspopenssl -o $@

testhttps: testhttps.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspopenssl -o $@

clean:
	@( $(RM) *.o vgcore.* core core.* $(TARGET) )

#--------------------------------------------------------------------

# make rule
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@	

%.o : %.cpp
	$(CC) $(CFLAGS) -c $^ -o $@	

