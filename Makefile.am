
AUTOMAKE_OPTIONS=subdir-objects

PREFIX=/usr
AM_CFLAGS=-std=gnu99 `pkg-config --cflags uuid blkid`
AM_LDFLAGS=`pkg-config --libs uuid blkid` -L$(top_builddir)


bin_PROGRAMS=make-bcache \
	     probe-bcache \
	     bcache-super-show \
	     bcachectl \
	     bcacheadm

noinst_PROGRAMS=bcache-test

lib_LIBRARIES=libbcache.a
libbcache_a_SOURCES=bcache.c
bcache_LDADD=libbcache.a

make_bcache_SOURCES=make-bcache.c
make_bcache_LDADD=libbcache.a

probe_bcache_SOURCES=probe-bcache.c
probe_bcache_LDADD=libbcache.a

bcache_super_show_SOURCES=bcache-super-show.c
bcache_super_show_LDADD=libbcache.a

bcachectl_SOURCES=bcachectl.c

bcache_test_SOURCES=bcache-test.c
bcache_test_LDFLAGS=-lm `pkg-config --libs openssl`
bcache_test_CFLAGS=$(AM_CFLAGS) `pkg-config --cflags openssl`

bcacheadm_SOURCES=bcacheadm.c
bcacheadm_CFLAGS=$(AM_CFLAGS) -g
bcacheadm_LDFLAGS=$(AM_LDFLAGS) -lnih
bcacheadm_LDADD=libbcache.a


udevrule_DATA=69-bcache.rules
udevruledir=$(prefix)/lib/udev/rules.d

udevr_DATA=bcache-register
udevrdir=$(prefix)/lib/udev/

initramfs_SCRIPTS=initramfs/bcache
initramfsdir=$(prefix)/etc/initramfs-tools/hooks/

man8_MANS= bcache-super-show.8 \
	   make-bcache.8 \
	   probe-bcache.8 \
	   bcacheadm.8
