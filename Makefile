
PREFIX=/usr
INSTALL=install
CFLAGS+=-std=gnu99 -O2 -g -MMD -Wall				\
	-Wno-pointer-sign					\
	-fno-strict-aliasing					\
	-I. -Iinclude -Ilibbcachefs				\
	-D_FILE_OFFSET_BITS=64					\
	-D_GNU_SOURCE						\
	-D_LGPL_SOURCE						\
	-DRCU_MEMBARRIER					\
	-DNO_BCACHE_CHARDEV					\
	-DNO_BCACHE_FS						\
	-DNO_BCACHE_SYSFS					\
	$(EXTRA_CFLAGS)
LDFLAGS+=-O2 -g

CC_VERSION=$(shell $(CC) -v 2>&1|grep -E '(gcc|clang) version')

ifneq (,$(findstring gcc,$(CC_VERSION)))
	CFLAGS+=-Wno-unused-but-set-variable
ifndef D
	CFLAGS+=-flto
	LDFLAGS+=-flto
endif
endif

ifneq (,$(findstring clang,$(CC_VERSION)))
	CFLAGS+=-Wno-missing-braces
endif

ifdef D
	CFLAGS+=-Werror
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
all: bcachefs

SRCS=bcachefs.c				\
     cmd_assemble.c			\
     cmd_debug.c			\
     cmd_device.c			\
     cmd_fs.c				\
     cmd_fsck.c				\
     cmd_format.c			\
     cmd_key.c				\
     cmd_migrate.c			\
     cmd_run.c				\
     crypto.c				\
     libbcachefs.c			\
     qcow2.c				\
     tools-util.c			\
     libbcachefs/alloc.c		\
     libbcachefs/bkey.c			\
     libbcachefs/bkey_methods.c		\
     libbcachefs/bset.c			\
     libbcachefs/btree_cache.c		\
     libbcachefs/btree_gc.c		\
     libbcachefs/btree_io.c		\
     libbcachefs/btree_iter.c		\
     libbcachefs/btree_update.c		\
     libbcachefs/buckets.c		\
     libbcachefs/checksum.c		\
     libbcachefs/clock.c		\
     libbcachefs/compress.c		\
     libbcachefs/debug.c		\
     libbcachefs/dirent.c		\
     libbcachefs/error.c		\
     libbcachefs/extents.c		\
     libbcachefs/fsck.c			\
     libbcachefs/inode.c		\
     libbcachefs/io.c			\
     libbcachefs/journal.c		\
     libbcachefs/keylist.c		\
     libbcachefs/migrate.c		\
     libbcachefs/move.c			\
     libbcachefs/movinggc.c		\
     libbcachefs/opts.c			\
     libbcachefs/siphash.c		\
     libbcachefs/six.c			\
     libbcachefs/super.c		\
     libbcachefs/super-io.c		\
     libbcachefs/tier.c			\
     libbcachefs/trace.c		\
     libbcachefs/util.c			\
     libbcachefs/xattr.c		\
     $(wildcard linux/*.c linux/*/*.c)	\
     $(wildcard ccan/*/*.c)

DEPS=$(SRCS:.c=.d)
-include $(DEPS)

OBJS=$(SRCS:.c=.o)
bcachefs: $(OBJS)

.PHONY: install
install: bcachefs
	mkdir -p $(DESTDIR)$(ROOT_SBINDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man8/
	$(INSTALL) -m0755 bcachefs	$(DESTDIR)$(ROOT_SBINDIR)
	$(INSTALL) -m0755 fsck.bcachefs	$(DESTDIR)$(ROOT_SBINDIR)
	$(INSTALL) -m0755 mkfs.bcachefs	$(DESTDIR)$(ROOT_SBINDIR)
	$(INSTALL) -m0644 bcachefs.8	$(DESTDIR)$(PREFIX)/share/man/man8/

.PHONY: clean
clean:
	$(RM) bcachefs $(OBJS) $(DEPS)

.PHONY: deb
deb: all
	debuild --unsigned-source	\
		--unsigned-changes	\
		--no-pre-clean		\
		--build=binary		\
		--diff-ignore		\
		--tar-ignore

.PHONE: update-bcachefs-sources
update-bcachefs-sources:
	echo `cd $(LINUX_DIR); git rev-parse HEAD` > .bcachefs_revision
	cp $(LINUX_DIR)/fs/bcachefs/*.[ch] libbcachefs/
	cp $(LINUX_DIR)/include/trace/events/bcachefs.h include/trace/events/

.PHONE: update-commit-bcachefs-sources
update-commit-bcachefs-sources: update-bcachefs-sources
	git commit -m "Update bcachefs sources to `cut -b1-10 .bcachefs_revision`"	\
		.bcachefs_revision libbcachefs/
