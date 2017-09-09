LDFLAGS=

.PHONY: all clean distclean install tvsat uninstall

KERNEL_VERSION:=$(shell uname -r)
MOD_DIR:=/lib/modules/$(KERNEL_VERSION)

ifdef KERNELRELEASE
	obj-m        := natbus.o tvsat.o
	natbus-objs  := natbus_mod.o
	tvsat-objs   := tvsat_mod.o
else
	LINUX_SRC_F=/usr/src/linux-$(KERNEL_VERSION)
	LINUX_SRC_F2=/usr/src/linux-source-$(KERNEL_VERSION)
	LINUX_SRC_H=/usr/src/linux-headers-$(KERNEL_VERSION)
	LINUX_SRC=$(shell if test -d '/usr/src/linux'; then\
			readlink -f '/usr/src/linux';\
			else if test -d $(LINUX_SRC_F); then\
				echo $(LINUX_SRC_F);\
				else if test -d $(LINUX_SRC_F2); then\
					echo $(LINUX_SRC_F2);\
					else if test -d $(LINUX_SRC_H); then\
						echo $(LINUX_SRC_H);\
						else echo '';\
					fi;\
				fi;\
			fi;\
		fi)
endif

EXTRA_CFLAGS+=-Idrivers/media/dvb/dvb-core
EXTRA_CFLAGS+=-Wall

all: tvsat

check:
	if test -z '$(LINUX_SRC)'; then\
		echo "!! Failed to find kernel sources/headers in the following locations:";\
		echo "!! the /usr/src/linux symbolic link";\
		echo "!! $(LINUX_SRC_F)";\
		echo "!! $(LINUX_SRC_F2)";\
		echo "!! $(LINUX_SRC_H)";\
		false;\
	fi

clean: check
	echo "* Cleaning module build"
	$(RM) modules.order
	$(RM) Module.markers
	$(MAKE) -C $(LINUX_SRC) M=$(shell pwd) KBUILD_VERBOSE=1 clean

distclean: clean

install:
	echo "* Installing kernel modules"
	install -d $(MOD_DIR)/tvsat
	cp natbus.ko $(MOD_DIR)/tvsat/
	cp tvsat.ko $(MOD_DIR)/tvsat/
	
	echo "* Calculating module dependencies"
	depmod -a
	
	if test -w "/etc/modules"; then\
		if test -z "`grep ^tvsat$$ /etc/modules`"; then\
			echo "* Adding /etc/modules entry";\
			echo tvsat >> /etc/modules;\
		fi;\
	else if test -w "/etc/init.d/boot.local"; then\
			if test -z "`grep "^modprobe tvsat$$" /etc/init.d/boot.local`"; then\
				echo "* Adding /etc/init.d/boot.local entry";\
				echo "modprobe tvsat" >> /etc/init.d/boot.local;\
			fi;\
		fi;\
	fi

tvsat: check
	-if test -z "`echo $(LINUX_SRC) |grep headers`"; then\
		echo "* Preparing kernel sources";\
		$(MAKE) -C $(LINUX_SRC) modules_prepare;\
	fi
	
	echo "* Building TV Sat kernel modules"
	$(MAKE) -C $(LINUX_SRC) M=$(shell pwd) KBUILD_VERBOSE=1 modules

uninstall:
	-if test -n "`lsmod |grep tvsat`"; then\
		modprobe -r tvsat;\
	fi
	
	echo "* Removing TV Sat kernel modules"
	-$(RM) $(MOD_DIR)/tvsat/natbus.ko
	-$(RM) $(MOD_DIR)/tvsat/tvsat.ko
	-if test -d "$(MOD_DIR)/tvsat" && test -w "$(MOD_DIR)/tvsat"; then\
		rmdir $(MOD_DIR)/tvsat;\
	fi
	
	if test -w "/etc/modules"; then\
		echo "* Removing entry from /etc/modules";\
		cp /etc/modules /etc/modules.bak;\
		sed /^tvsat$$/d /etc/modules.bak > /etc/modules;\
		rm /etc/modules.bak;\
	else if test -w "/etc/init.d/boot.local"; then\
			echo "* Removing entry from /etc/init.d/boot.local";\
			cp /etc/init.d/boot.local /etc/init.d/boot.local.bak;\
			sed /"^modprobe tvsat$$"/d /etc/init.d/boot.local.bak > /etc/init.d/boot.local;\
			chmod +x /etc/init.d/boot.local;\
			rm /etc/init.d/boot.local.bak;\
		fi;\
	fi
	
	echo "* Calculating module dependencies"
	-depmod -a
