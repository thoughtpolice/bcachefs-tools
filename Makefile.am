
AUTOMAKE_OPTIONS=subdir-objects

PREFIX=/usr
AM_CFLAGS=-std=gnu99 -O2 -Wall -Werror -g

bin_PROGRAMS=bcacheadm probe-bcache

bcache.o: CFLAGS += `pkg-config --cflags uuid blkid`

bcacheadm_SOURCES=bcacheadm.c
bcacheadm_CFLAGS=$(AM_CFLAGS) `pkg-config --cflags uuid blkid libnih`
bcacheadm_LDFLAGS=`pkg-config --libs uuid blkid libnih`
bcacheadm_LDADD=bcache.o

probe_bcache_SOURCES=probe-bcache.c
probe_bcache_CFLAGS=$(AM_CFLAGS) `pkg-config --cflags uuid blkid`
probe_bcache_LDFLAGS=`pkg-config --libs uuid blkid`
probe_bcache_LDADD=bcache.o

udevrule_DATA=69-bcache.rules
udevruledir=$(prefix)/lib/udev/rules.d

udevr_DATA=bcache-register
udevrdir=$(prefix)/lib/udev/

initramfs_SCRIPTS=initramfs/bcache
initramfsdir=$(prefix)/etc/initramfs-tools/hooks/

man8_MANS=bcacheadm.8 probe-bcache.8

include ../../../../../../../../../../usr/share/DateraContainer/Makefile.rpm
# some version of automake don't like absolute path names for included makefiles
