
#--------------------------------------------------------------------

CC = gcc
AR = ar cru
CFLAGS = -Wall -D_REENTRANT -D_GNU_SOURCE -g -fPIC
SOFLAGS = -shared
LDFLAGS = -lstdc++ -lpthread -lresolv

LINKER = $(CC)
LINT = lint -c
RM = /bin/rm -f

MASSL_INCL = -I$(HOME)/matrixssl-1-8-3-open/
MASSL_LIB  = -L$(HOME)/matrixssl-1-8-3-open/src -lmatrixssl

LIBEVENT_LIB  = -L$(HOME)/libevent -levent

SPSERVER_INCL = -I../spserver
SPSERVER_LIB  = -L../spserver -lspserver

CFLAGS  += $(MASSL_INCL) $(SPSERVER_INCL)
LDFLAGS += $(MASSL_LIB)  $(SPSERVER_LIB)  $(LIBEVENT_LIB)

#--------------------------------------------------------------------

LIBOBJS = sslSocket.o spmatrixssl.o

TARGET =  libspmatrixssl.so \
	testechos

#--------------------------------------------------------------------

all: $(TARGET)

libspmatrixssl.so: $(LIBOBJS)
	$(LINKER) $(SOFLAGS) $^ -o $@

testechos: testechos.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspmatrixssl -o $@

clean:
	@( $(RM) *.o vgcore.* core core.* $(TARGET) )

#--------------------------------------------------------------------

# make rule
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@	

%.o : %.cpp
	$(CC) $(CFLAGS) -c $^ -o $@	

