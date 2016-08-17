
PREFIX=/usr
INSTALL=install
CFLAGS+=-std=gnu99 -O2 -Wall -g -D_FILE_OFFSET_BITS=64 -I.
LDFLAGS+=-static

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
	$(RM) -f bcache *.o *.a

CCANSRCS=$(wildcard ccan/*/*.c)
CCANOBJS=$(patsubst %.c,%.o,$(CCANSRCS))

libccan.a: $(CCANOBJS)
	$(AR) r $@ $(CCANOBJS)

util.o: CFLAGS += `pkg-config --cflags blkid uuid`
bcache.o: CFLAGS += `pkg-config --cflags libnih`

bcache-objs = bcache.o bcache-assemble.o bcache-device.o bcache-format.o\
	bcache-fs.o bcache-run.o bcache-key.o libbcache.o crypto.o

bcache: LDLIBS += `pkg-config --libs uuid blkid libnih` -lscrypt -lsodium -lkeyutils
bcache: $(bcache-objs) util.o libccan.a

bcache-test: LDLIBS += `pkg-config --libs openssl`

deb:
	debuild -nc -us -uc -i -I
