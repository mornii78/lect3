MYRPC_DEB_DIR    := $(CURDIR)/build/deb
MYSYSLOG_DEB_DIR := $(CURDIR)/../mysyslog/build/deb
REPO_DIR ?= $(CURDIR)/../myRPC-repo

.PHONY: repository
.PHONY: all clean deb

all:
	$(MAKE) -C libmysyslog
	$(MAKE) -C myRPCclient
	$(MAKE) -C myRPCserver

clean:
	$(MAKE) -C libmysyslog clean
	$(MAKE) -C myRPCclient clean
	$(MAKE) -C myRPCserver clean
	rm -rf build-deb
	rm -rf build
	rm -rf bin
	rm -rf repo
	rm -rf deb
	rm -rf repository

deb:
	$(MAKE) -C libmysyslog deb
	$(MAKE) -C myRPCclient deb
	$(MAKE) -C myRPCserver deb

repo:
	mkdir -p repo
	cp deb/*.deb repo/
	dpkg-scanpackages repo /dev/null | gzip -9c > repo/Packages.gz
