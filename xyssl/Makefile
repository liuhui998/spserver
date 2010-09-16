
#--------------------------------------------------------------------

CC = gcc
AR = ar cru
CFLAGS = -Wall -D_REENTRANT -D_GNU_SOURCE -g -fPIC
SOFLAGS = -shared
LDFLAGS = -lstdc++ -lpthread -lresolv

LINKER = $(CC)
LINT = lint -c
RM = /bin/rm -f

XYSSL_INCL = -I$(HOME)/xyssl-0.9/include
XYSSL_LIB  = -L$(HOME)/xyssl-0.9/library -lxyssl

LIBEVENT_LIB  = -L$(HOME)/libevent -levent

SPSERVER_INCL = -I../spserver
SPSERVER_LIB  = -L../spserver -lspserver

CFLAGS  += $(XYSSL_INCL) $(SPSERVER_INCL)
LDFLAGS += $(LIBEVENT_LIB) $(XYSSL_LIB) $(SPSERVER_LIB)

#--------------------------------------------------------------------

LIBOBJS = spxyssl.o

TARGET =  libspxyssl.so \
	testechos

#--------------------------------------------------------------------

all: $(TARGET)

libspxyssl.so: $(LIBOBJS)
	$(LINKER) $(SOFLAGS) $^ -o $@

testechos: testechos.o
	$(LINKER) -L. -lspxyssl $(LDFLAGS) $^ -o $@

clean:
	@( $(RM) *.o vgcore.* core core.* $(TARGET) )

#--------------------------------------------------------------------

# make rule
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@	

%.o : %.cpp
	$(CC) $(CFLAGS) -c $^ -o $@	

