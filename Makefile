
PREFIX=/usr
INSTALL=install
CFLAGS+=-std=gnu99 -O2 -Wall -g -D_FILE_OFFSET_BITS=64 -I.
LDFLAGS+=-static

PKGCONFIG_LIBS="blkid uuid libnih"
CFLAGS+=`pkg-config --cflags	${PKGCONFIG_LIBS}`
LDLIBS+=`pkg-config --libs	${PKGCONFIG_LIBS}` -lscrypt -lsodium -lkeyutils

ifeq ($(PREFIX), "/usr")
	ROOT_SBINDIR=/sbin
else
	ROOT_SBINDIR=$(PREFIX)/sbin
endif

all: bcache

install: bcache
	$(INSTALL) -m0755 bcache $(DESTDIR)$(ROOT_SBINDIR)
	$(INSTALL) -m0644 -- bcache.8 $(DESTDIR)$(PREFIX)/share/man/man8/

clean:
	$(RM) bcache *.o *.a

CCANSRCS=$(wildcard ccan/*/*.c)
CCANOBJS=$(patsubst %.c,%.o,$(CCANSRCS))

libccan.a: $(CCANOBJS)
	$(AR) r $@ $(CCANOBJS)

bcache-objs = bcache.o bcache-assemble.o bcache-device.o bcache-format.o\
	bcache-fs.o bcache-run.o bcache-key.o libbcache.o crypto.o util.o

bcache: $(bcache-objs) libccan.a

deb:
	debuild -nc -us -uc -i -I
