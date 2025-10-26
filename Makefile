VERSION=0.1.0
TARBALL=corsair-lncore-dkms_$(VERSION).orig.tar.gz

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/src modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/src clean

tarball: clean
	git archive --prefix=corsair-lncore-dkms-$(VERSION)/ --format=tar HEAD | gzip > $(TARBALL)

.PHONY: all clean tarball
