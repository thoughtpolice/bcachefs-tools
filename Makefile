
PREFIX=/usr
UDEVLIBDIR=/lib/udev
DRACUTLIBDIR=/lib/dracut
INSTALL=install
CFLAGS+=-std=gnu99 -O2 -Wall -Werror -g -I.

all: bcacheadm probe-bcache

install: bcacheadm probe-bcache
	$(INSTALL) -m0755 bcacheadm $(DESTDIR)${PREFIX}/sbin/
	$(INSTALL) -m0755 probe-bcache bcache-register		$(DESTDIR)$(UDEVLIBDIR)/
	$(INSTALL) -m0644 69-bcache.rules	$(DESTDIR)$(UDEVLIBDIR)/rules.d/
	#-$(INSTALL) -T -m0755 initramfs/hook	$(DESTDIR)/usr/share/initramfs-tools/hooks/bcache
	if [ -d $(DESTDIR)$(DRACUTLIBDIR)/modules.d ]; then\
		$(INSTALL) -D -m0755 dracut/module-setup.sh $(DESTDIR)$(DRACUTLIBDIR)/modules.d/90bcache/module-setup.sh; \
	fi
	$(INSTALL) -m0644 -- *.8 $(DESTDIR)${PREFIX}/share/man/man8/

clean:
	$(RM) -f probe-bcache bcacheadm bcache-test *.o

bcache.o: CFLAGS += `pkg-config --cflags uuid blkid`

bcacheadm.o: CFLAGS += `pkg-config --cflags uuid blkid libnih`
bcacheadm: LDLIBS += `pkg-config --libs uuid blkid libnih`
bcacheadm: bcache.o

probe-bcache.o: CFLAGS += `pkg-config --cflags uuid blkid`
probe-bcache: LDLIBS += `pkg-config --libs uuid blkid`

bcache-test: LDLIBS += `pkg-config --libs openssl`
