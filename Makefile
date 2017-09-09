.PHONY: all clean distclean install reinstall uninstall
.SILENT:

all clean distclean install uninstall: check
	$(MAKE) -s -f Makefile.silent $@

check:
	if [ `id -u` -ne '0' ]; then echo "You need to have root privileges !"; false; fi

reinstall: check
	$(MAKE) -s -f Makefile.silent uninstall
	$(MAKE) -s -f Makefile.silent install
