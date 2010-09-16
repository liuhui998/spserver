
#--------------------------------------------------------------------

CC = gcc
AR = ar cru
CFLAGS = -Wall -D_REENTRANT -D_GNU_SOURCE -g -fPIC
SOFLAGS = -shared
LDFLAGS = -lstdc++ -lpthread -lresolv

LINKER = $(CC)
LINT = lint -c
RM = /bin/rm -f

GNUTLS_INCL = -I/usr/local/include/
GNUTLS_LIB  = -L/usr/local/lib -lgcrypt -lgpg-error -lgnutls -lgnutls-extra -lstdc++

LIBEVENT_LIB  = -L$(HOME)/libevent -levent

SPSERVER_INCL = -I../spserver
SPSERVER_LIB  = -L../spserver -lspserver

CFLAGS  += $(GNUTLS_INCL) $(SPSERVER_INCL)
LDFLAGS += $(LIBEVENT_LIB) $(GNUTLS_LIB) $(SPSERVER_LIB)

#--------------------------------------------------------------------

LIBOBJS = spgnutls.o spgcrypt.o

TARGET =  libspgnutls.so \
	testechos

#--------------------------------------------------------------------

all: $(TARGET)

libspgnutls.so: $(LIBOBJS)
	$(LINKER) $(SOFLAGS) $^ -o $@

testechos: testechos.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspgnutls -o $@

clean:
	@( $(RM) *.o vgcore.* core core.* $(TARGET) )

#--------------------------------------------------------------------

# make rule
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@	

%.o : %.cpp
	$(CC) $(CFLAGS) -c $^ -o $@	

