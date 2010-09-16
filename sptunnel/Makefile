
#--------------------------------------------------------------------

CC = gcc
AR = ar cru
CFLAGS = -Wall -D_REENTRANT -D_GNU_SOURCE -g -fPIC
SOFLAGS = -shared
LDFLAGS = -lstdc++ -lpthread -lresolv

LINKER = $(CC)
LINT = lint -c
RM = /bin/rm -f

ifeq ($(origin ssl), undefined)
	ssl = openssl
endif

ifeq ($(ssl), openssl)
	CFLAGS += -DOPENSSL
	SSL_INCL = -I../openssl
	SSL_LIB  = -L../openssl -lspopenssl -lssl -lcrypto
else
	CFLAGS += -DMATRIXSSL
	SSL_INCL = -I../matrixssl
	SSL_LIB  = -L../matrixssl -lspmatrixssl -lmatrixssl
endif

LIBEVENT_LIB  = -L$(HOME)/libevent -levent

SPSERVER_INCL = -I../spserver
SPSERVER_LIB  = -L../spserver -lspserver

CFLAGS  += $(SSL_INCL) $(SPSERVER_INCL)
LDFLAGS += $(LIBEVENT_LIB) $(SSL_LIB) $(SPSERVER_LIB)

#--------------------------------------------------------------------

TARGET =  sptunnel 

#--------------------------------------------------------------------

all: $(TARGET)

sptunnel: sptunnelimpl.o sptunnel.o
	$(LINKER) $(LDFLAGS) $^ -o $@

clean:
	@( $(RM) *.o vgcore.* core core.* $(TARGET) )

#--------------------------------------------------------------------

# make rule
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@	

%.o : %.cpp
	$(CC) $(CFLAGS) -c $^ -o $@	

