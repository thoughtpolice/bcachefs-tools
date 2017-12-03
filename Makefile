
PREFIX=/usr/local
INSTALL=install
CFLAGS+=-std=gnu89 -O2 -g -MMD -Wall				\
	-Wno-pointer-sign					\
	-fno-strict-aliasing					\
	-I. -Iinclude						\
	-D_FILE_OFFSET_BITS=64					\
	-D_GNU_SOURCE						\
	-D_LGPL_SOURCE						\
	-DRCU_MEMBARRIER					\
	-DZSTD_STATIC_LINKING_ONLY				\
	-DNO_BCACHEFS_CHARDEV					\
	-DNO_BCACHEFS_FS					\
	-DNO_BCACHEFS_SYSFS					\
	-DVERSION_STRING='"$(VERSION)"'				\
	$(EXTRA_CFLAGS)
LDFLAGS+=$(CFLAGS)

VERSION?=$(shell git describe --dirty 2>/dev/null || echo 0.1-nogit)

CC_VERSION=$(shell $(CC) -v 2>&1|grep -E '(gcc|clang) version')

ifneq (,$(findstring gcc,$(CC_VERSION)))
	CFLAGS+=-Wno-unused-but-set-variable
endif

ifneq (,$(findstring clang,$(CC_VERSION)))
	CFLAGS+=-Wno-missing-braces
endif

ifdef D
	CFLAGS+=-Werror
	CFLAGS+=-DCONFIG_BCACHEFS_DEBUG=y
endif

PKGCONFIG_LIBS="blkid uuid liburcu libsodium zlib liblz4 libzstd"

CFLAGS+=`pkg-config --cflags	${PKGCONFIG_LIBS}`
LDLIBS+=`pkg-config --libs	${PKGCONFIG_LIBS}`

LDLIBS+=-lm -lpthread -lrt -lscrypt -lkeyutils -laio

ifeq ($(PREFIX),/usr)
	ROOT_SBINDIR=/sbin
	INITRAMFS_DIR=$(PREFIX)/share/initramfs-tools
else
	ROOT_SBINDIR=$(PREFIX)/sbin
	INITRAMFS_DIR=/etc/initramfs-tools
endif

.PHONY: all
all: bcachefs

SRCS=$(shell find . -type f -iname '*.c')
DEPS=$(SRCS:.c=.d)
-include $(DEPS)

OBJS=$(SRCS:.c=.o)
bcachefs: $(OBJS)

# If the version string differs from the last build, update the last version
ifneq ($(VERSION),$(shell cat .version 2>/dev/null))
.PHONY: .version
endif
.version:
	echo '$(VERSION)' > $@

# Rebuild the 'version' command any time the version string changes
cmd_version.o : .version

MAN_PODS=$(shell find man -type f -iname '*.pod')
MAN_TROFFS=$(patsubst %.pod,%,$(MAN_PODS))
MAN_HTML=$(patsubst %.pod,%.html,$(MAN_PODS))
MAN_HTML+=man/index.html
# EXTRAS is just a list of symlinks for making things nicer
# for the user
MAN_EXTRAS=\
	man/mkfs.bcachefs.8 \
	man/fsck.bcachefs.8

man/fsck.bcachefs.8: man/bcachefs-fsck.8
	cp $< $@
man/mkfs.bcachefs.8: man/bcachefs-format.8
	cp $< $@
man/index.html: man/bcachefs.8.html
	cp $< $@

man/%.html: man/%.pod man/bcachefs-footer.in
	cat $^ | \
	perl -pe 's/L<(.+)\|bcachefs([0-9a-z_\-]*?)\((\d+)\)>/L<$$1|bcachefs$$2\.$$3>/g' | \
	perl -pe 's/L<bcachefs([0-9a-z_\-]*)\((\d+)\)>/L<bcachefs$$1\($$2\)\|bcachefs$$1\.$$2>/g' | \
	pod2html \
		--podroot=$(PWD)/man \
		--podpath=. \
		--htmldir="./" \
		--css=base.css | \
	perl -pe 's#href="/bcachefs#href="bcachefs#g' \
	> $@

man/%: man/%.pod man/bcachefs-footer.in
	cat $^ | pod2man \
		--section=$(shell echo "$<" | awk -F'.' '{print $$2}') \
		--name=$(basename $(notdir $@)) \
		--center="bcachefs User Manual" \
		> $@

.PHONY: man-html sync-html
man-html: $(MAN_HTML)

sync-html: man-html
	rsync --delete-excluded -arvz man/*.css man/*.html $(RSYNC_HOST)

.PHONY: man
man: $(MAN_TROFFS) $(MAN_EXTRAS)

.PHONY: install
install: bcachefs man
	mkdir -p $(DESTDIR)$(ROOT_SBINDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man8
	$(INSTALL) -m0755 bcachefs	$(DESTDIR)$(ROOT_SBINDIR)
	$(INSTALL) -m0755 fsck.bcachefs	$(DESTDIR)$(ROOT_SBINDIR)
	$(INSTALL) -m0755 mkfs.bcachefs	$(DESTDIR)$(ROOT_SBINDIR)
	$(INSTALL) -m0755 -D initramfs/hook $(DESTDIR)$(INITRAMFS_DIR)/hooks/bcachefs
	echo "copy_exec $(ROOT_SBINDIR)/bcachefs /sbin/bcachefs" >> $(DESTDIR)$(INITRAMFS_DIR)/hooks/bcachefs
	$(INSTALL) -m0755 -D initramfs/script $(DESTDIR)$(INITRAMFS_DIR)/scripts/local-premount/bcachefs
	$(INSTALL) -m0644 $(MAN_EXTRAS) $(DESTDIR)$(PREFIX)/share/man/man8
	for x in $(MAN_TROFFS); do \
		section=$$(echo "$$x" | awk -F'.' '{print $$2}'); \
		mkdir -p $(DESTDIR)$(PREFIX)/share/man/man$$section; \
		$(INSTALL) -m0644 $$x $(DESTDIR)$(PREFIX)/share/man/man$$section; \
	done

.PHONY: clean
clean:
	$(RM) bcachefs $(OBJS) $(DEPS) $(MAN_TROFFS) $(MAN_HTML) $(MAN_EXTRAS)

.PHONY: deb
deb: all
# --unsigned-source --unsigned-changes --no-pre-clean --build=binary
# --diff-ignore --tar-ignore
	debuild -us -uc -nc -b -i -I

.PHONE: update-bcachefs-sources
update-bcachefs-sources:
	git rm -rf --ignore-unmatch libbcachefs
	cp $(LINUX_DIR)/fs/bcachefs/*.[ch] libbcachefs/
	cp $(LINUX_DIR)/include/trace/events/bcachefs.h include/trace/events/
	echo `cd $(LINUX_DIR); git rev-parse HEAD` > .bcachefs_revision
	git add libbcachefs/*.[ch] include/trace/events/bcachefs.h .bcachefs_revision

.PHONE: update-commit-bcachefs-sources
update-commit-bcachefs-sources: update-bcachefs-sources
	git commit -m "Update bcachefs sources to `cd $(LINUX_DIR); git show --oneline --no-patch`"
