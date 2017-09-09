CXXFLAGS+=-g -Wall
LDFLAGS+=-pthread
BIN_DIR=/usr/bin

.PHONY: all clean distclean install tvsatctl tvsatcfg uninstall

MANPATH:=$(shell manpath|awk -F: {'print $$1'} )

all: tvsatctl tvsatcfg

clean:
	echo "* Cleaning app build"
	$(RM) *.o tvsatctl tvsatcfg

distclean: clean

install:
	echo "* Installing control daemon"
	install -o0 -g0 -d $(BIN_DIR)
	install -m 0755 -o0 -g0 tvsatcfg $(BIN_DIR)/tvsatcfg
	
	echo "* Installing configuration tool"
	install -m 0744 -o0 -g0 tvsatctl $(BIN_DIR)/tvsatd
	
	-if test -d "$(MANPATH)"; then\
		install -o0 -g0 -d $(MANPATH)/man1;\
		install -m 0644 -o0 -g0 tvsatcfg.1.gz $(MANPATH)/man1/tvsatcfg.1.gz;\
	fi

tvsatctl: config.o discover.o log.o rawsocket.o streamin.o tvsatctl.o tvsatmgr.o udpsocket.o
	echo "* Building control daemon"
	$(CXX) $(LDFLAGS) config.o discover.o log.o rawsocket.o streamin.o tvsatctl.o tvsatmgr.o udpsocket.o -o $@

tvsatcfg: discover.o log.o rawsocket.o tvsatcfg.o udpsocket.o
	echo "* Building configuration tool"
	$(CXX) $(LDFLAGS) discover.o log.o rawsocket.o tvsatcfg.o udpsocket.o -o $@

uninstall:
	-if test -n "`ps -A |grep tvsatd`"; then\
		echo "* Stopping control daemon";\
		killall -q tvsatd;\
		for i in 1 2 3 4 5; do\
			echo -n .;\
			sleep 1;\
		done;\
		echo;\
		killall -9 -q tvsatd;\
	fi
	
	echo "* Removing control daemon"
	-$(RM) $(BIN_DIR)/tvsatd
	
	echo "* Removing configuration tool"
	-$(RM) $(BIN_DIR)/tvsatcfg
	-$(RM) $(MANPATH)/man1/tvsatcfg.1.gz
