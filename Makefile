
PREFIX=/usr
UDEVLIBDIR=/lib/udev
DRACUTLIBDIR=/lib/dracut
INSTALL=install
CFLAGS+=-std=gnu99 -O2 -Wall -g -D_FILE_OFFSET_BITS=64 -I.
LDFLAGS+=-static

all: bcache probe-bcache

install: bcache probe-bcache
	$(INSTALL) -m0755 bcache $(DESTDIR)${PREFIX}/sbin/
	#$(INSTALL) -m0755 probe-bcache bcache-register		$(DESTDIR)$(UDEVLIBDIR)/
	#$(INSTALL) -m0644 69-bcache.rules	$(DESTDIR)$(UDEVLIBDIR)/rules.d/
	#-$(INSTALL) -T -m0755 initramfs/hook	$(DESTDIR)/usr/share/initramfs-tools/hooks/bcache
	if [ -d $(DESTDIR)$(DRACUTLIBDIR)/modules.d ]; then\
		$(INSTALL) -D -m0755 dracut/module-setup.sh $(DESTDIR)$(DRACUTLIBDIR)/modules.d/90bcache/module-setup.sh; \
	fi
	$(INSTALL) -m0644 -- *.8 $(DESTDIR)${PREFIX}/share/man/man8/

clean:
	$(RM) -f probe-bcache bcache bcache-test *.o *.a

CCANSRCS=$(wildcard ccan/*/*.c)
CCANOBJS=$(patsubst %.c,%.o,$(CCANSRCS))

libccan.a: $(CCANOBJS)
	$(AR) r $@ $(CCANOBJS)

util.o: CFLAGS += `pkg-config --cflags blkid uuid`
bcache.o: CFLAGS += `pkg-config --cflags libnih`

bcache-objs = bcache.o bcache-assemble.o bcache-device.o bcache-format.o\
	bcache-fs.o bcache-run.o

# have to build ccan first, as headers require config.h to be generated:
#$(bcache-objs): ccan/libccan.a

bcache: LDLIBS += `pkg-config --libs uuid blkid libnih`
bcache: $(bcache-objs) util.o libccan.a

probe-bcache.o: CFLAGS += `pkg-config --cflags uuid blkid`
probe-bcache: LDLIBS += `pkg-config --libs uuid blkid`

bcache-test: LDLIBS += `pkg-config --libs openssl`
