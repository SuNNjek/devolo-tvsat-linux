.PHONY: all clean distclean install reinstall uninstall
.SILENT:

all distclean: check
	$(MAKE) -s -f Makefile.silent $@

clean clean_module clean_app: check
	$(MAKE) -s -f Makefile.silent $@

install install_module install_app install_init: check
	$(MAKE) -s -f Makefile.silent $@

uninstall uninstall_module uninstall_app uninstall_init: check
	$(MAKE) -s -f Makefile.silent $@

check:
	if [ `id -u` -ne '0' ]; then echo "You need to have root privileges !"; false; fi

reinstall: check
	$(MAKE) -s -f Makefile.silent uninstall
	$(MAKE) -s -f Makefile.silent install
