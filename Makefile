
PREFIX=/usr
INSTALL=install
CFLAGS+=-std=gnu99 -O2 -g -MMD -Wall				\
	-Wno-unused-but-set-variable				\
	-Wno-pointer-sign					\
	-fno-strict-aliasing					\
	-I. -Iinclude -Ilibbcache				\
	-D_FILE_OFFSET_BITS=64					\
	-D_GNU_SOURCE						\
	-D_LGPL_SOURCE						\
	-DRCU_MEMBARRIER					\
	-DNO_BCACHE_ACCOUNTING					\
	-DNO_BCACHE_BLOCKDEV					\
	-DNO_BCACHE_CHARDEV					\
	-DNO_BCACHE_FS						\
	-DNO_BCACHE_NOTIFY					\
	-DNO_BCACHE_WRITEBACK					\
	$(EXTRA_CFLAGS)
LDFLAGS+=-O2 -g

ifdef D
	CFLAGS+=-Werror
else
	CFLAGS+=-flto
	LDFLAGS+=-flto
endif

PKGCONFIG_LIBS="blkid uuid liburcu libsodium zlib"
CFLAGS+=`pkg-config --cflags	${PKGCONFIG_LIBS}`
LDLIBS+=`pkg-config --libs	${PKGCONFIG_LIBS}` 		\
	-lm -lpthread -lrt -lscrypt -lkeyutils

ifeq ($(PREFIX),/usr)
	ROOT_SBINDIR=/sbin
else
	ROOT_SBINDIR=$(PREFIX)/sbin
endif

.PHONY: all
all: bcache

CCANSRCS=$(wildcard ccan/*/*.c)
CCANOBJS=$(patsubst %.c,%.o,$(CCANSRCS))

# Linux kernel shim:
LINUX_SRCS=$(wildcard linux/*.c linux/*/*.c)
LINUX_OBJS=$(LINUX_SRCS:.c=.o)

OBJS=bcache.o			\
     bcache-userspace-shim.o	\
     cmd_assemble.o		\
     cmd_debug.o		\
     cmd_device.o		\
     cmd_fs.o			\
     cmd_fsck.o			\
     cmd_format.o		\
     cmd_key.o			\
     cmd_migrate.o		\
     cmd_run.o			\
     crypto.o			\
     libbcache.o		\
     qcow2.o			\
     tools-util.o		\
     $(LINUX_OBJS)		\
     $(CCANOBJS)

DEPS=$(OBJS:.o=.d)
-include $(DEPS)

bcache: $(OBJS)

.PHONY: install
install: bcache
	mkdir -p $(DESTDIR)$(ROOT_SBINDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man8/
	$(INSTALL) -m0755 bcache	$(DESTDIR)$(ROOT_SBINDIR)
	$(INSTALL) -m0755 mkfs.bcache	$(DESTDIR)$(ROOT_SBINDIR)
	$(INSTALL) -m0644 bcache.8	$(DESTDIR)$(PREFIX)/share/man/man8/

.PHONY: clean
clean:
	$(RM) bcache $(OBJS) $(DEPS)

.PHONY: deb
deb: all
	debuild --unsigned-source	\
		--unsigned-changes	\
		--no-pre-clean		\
		--build=binary		\
		--diff-ignore		\
		--tar-ignore

.PHONE: update-bcache-sources
update-bcache-sources:
	echo BCACHE_REVISION=`cd $(LINUX_DIR); git rev-parse HEAD` > .bcache_revision
	cp $(LINUX_DIR)/drivers/md/bcache/*.[ch] libbcache/
	cp $(LINUX_DIR)/include/trace/events/bcache.h include/trace/events/
	cp $(LINUX_DIR)/include/uapi/linux/bcache.h include/linux/
	cp $(LINUX_DIR)/include/uapi/linux/bcache-ioctl.h include/linux/
