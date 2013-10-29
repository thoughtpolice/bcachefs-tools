
AUTOMAKE_OPTIONS=subdir-objects

PREFIX=/usr
AM_CFLAGS=-O2 -Wall -g


bin_PROGRAMS=make-bcache \
	     probe-bcache \
	     bcache-super-show

noinst_PROGRAMS=bcache-test

make_bcache_SOURCES=make-bcache.c bcache.c
make_bcache_LDADD= -lm -lssl -lcrypto -luuid

probe_bcache_SOURCES=probe-bcache.c bcache.c
probe_bcache_LDADD=-luuid

bcache_super_show_SOURCES=bcache-super-show.c bcache.c
bcache_super_show_LDADD=-luuid

bcache_test_SOURCE=bcache-test.c
bcache_test_LDADD= -lm -lssl -lcrypto

udevrule_DATA=61-bcache.rules
udevruledir=$(prefix)/lib/udev/rules.d

udevr_DATA=bcache-register
udevrdir=$(prefix)/lib/udev/

initramfs_SCRIPTS=initramfs/bcache
initramfsdir=$(prefix)/etc/initramfs-tools/hooks/

man8_MANS= bcache-super-show.8 \
	   make-bcache.8 \
	   probe-bcache.8
