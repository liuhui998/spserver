
#--------------------------------------------------------------------

CC = gcc
AR = ar cru
CFLAGS = -Wall -D_REENTRANT -D_GNU_SOURCE -g -fPIC
SOFLAGS = -shared
LDFLAGS = -lstdc++ -lpthread

LINKER = $(CC)
LINT = lint -c
RM = /bin/rm -f

LIBEVENT_INCL = -I$(HOME)/libevent/
LIBEVENT_LIB  = -L$(HOME)/libevent -levent -lrt

CFLAGS  += $(LIBEVENT_INCL)
LDFLAGS += $(LIBEVENT_LIB)

#--------------------------------------------------------------------

LIBOBJS = sputils.o spioutils.o spiochannel.o \
	spthreadpool.o event_msgqueue.o spbuffer.o sphandler.o \
	spmsgblock.o spmsgdecoder.o spresponse.o sprequest.o \
	spexecutor.o spsession.o speventcb.o spserver.o \
	spdispatcher.o splfserver.o \
	sphttpmsg.o sphttp.o spsmtp.o

TARGET =  libspserver.so libspserver.a \
		testecho testthreadpool testsmtp testchat teststress testhttp \
		testhttp_d testhttpmsg testdispatcher testchat_d testunp

#--------------------------------------------------------------------

all: $(TARGET)

libspserver.so: $(LIBOBJS)
	$(LINKER) $(SOFLAGS) $^ -o $@

libspserver.a: $(LIBOBJS)
	$(AR) $@ $^

testthreadpool: testthreadpool.o
	$(LINKER) $^ -L. -lspserver $(LDFLAGS) -o $@

testsmtp: testsmtp.o
	$(LINKER) $^ -L. -lspserver $(LDFLAGS) -o $@
                             
testchat: testchat.o
	$(LINKER) $^ -L. -lspserver $(LDFLAGS) -o $@
                             
teststress: teststress.o
	$(LINKER) $^ -L. -levent $(LDFLAGS) -o $@
                             
testecho: testecho.o
	$(LINKER) $^ -L. -lspserver $(LDFLAGS) -o $@
                             
testhttp: testhttp.o
	$(LINKER) $^ -L. -lspserver $(LDFLAGS) -o $@
                             
testhttp_d: testhttp_d.o
	$(LINKER) $^ -L. -lspserver $(LDFLAGS) -o $@

testhttpmsg: sputils.o sphttpmsg.o testhttpmsg.o
	$(LINKER) $^ $(LDFLAGS) -o $@

testdispatcher: testdispatcher.o
	$(LINKER) $^ -L. -lspserver $(LDFLAGS) -o $@
                              
testchat_d: testchat_d.o
	$(LINKER) $^ -L. -lspserver $(LDFLAGS) -o $@
                              
testunp: testunp.o
	$(LINKER) $^ -L. -lspserver $(LDFLAGS) -o $@

clean:
	@( $(RM) *.o vgcore.* core core.* $(TARGET) )

#--------------------------------------------------------------------

# make rule
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@	

%.o : %.cpp
	$(CC) $(CFLAGS) -c $^ -o $@	

