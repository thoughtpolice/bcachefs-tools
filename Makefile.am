
AUTOMAKE_OPTIONS=subdir-objects

PREFIX=/usr
AM_CFLAGS=-O2 -Wall -g -std=gnu99


bin_PROGRAMS=make-bcache \
	     probe-bcache \
	     bcache-super-show \
	     bcachectl

noinst_PROGRAMS=bcache-test

make_bcache_SOURCES=make-bcache.c bcache.c
make_bcache_LDFLAGS=`pkg-config --libs uuid blkid`
make_bcache_CFLAGS=$(AM_CFLAGS) `pkg-config --cflags uuid blkid`

probe_bcache_SOURCES=probe-bcache.c bcache.c
probe_bcache_LDFLAGS=`pkg-config --libs uuid blkid`
probe_bcache_CFLAGS=$(AM_CFLAGS) `pkg-config --cflags uuid blkid`

bcache_super_show_SOURCES=bcache-super-show.c bcache.c
bcache_super_show_LDFLAGS=`pkg-config --libs uuid`
bcache_super_show_CFLAGS=$(AM_CFLAGS) `pkg-config --cflags uuid`

bcachectl_SOURCES=bcachectl.c

bcache_test_SOURCE=bcache-test.c
bcache_test_LDFLAGS=-lm `pkg-config --libs openssl`
bcache_test_CFLAGS=$(AM_CFLAGS) `pkg-config --cflags openssl`

udevrule_DATA=69-bcache.rules
udevruledir=$(prefix)/lib/udev/rules.d

udevr_DATA=bcache-register
udevrdir=$(prefix)/lib/udev/

initramfs_SCRIPTS=initramfs/bcache
initramfsdir=$(prefix)/etc/initramfs-tools/hooks/

man8_MANS= bcache-super-show.8 \
	   make-bcache.8 \
	   probe-bcache.8
