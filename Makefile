.PHONY: all clean distclean install reinstall uninstall

.PHONY: build_app clean_app install_app uninstall_app
.PHONY: build_module clean_module install_module uninstall_module
.PHONY: install_init uninstall_init

.SILENT:

all clean distclean: check
	$(MAKE) -s -f Makefile.silent $@

build_app clean_app:
	$(MAKE) -s -f Makefile.silent $@

build_module clean_module: check
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
