Summary: bcache-tools: manage local bcache devices
Name: %{package_name}
Version: 0.datera.%{datera_version}
Release: %{?release:%{release}}%{!?release:eng}
Source0: %{name}-%{version}.tar.gz
License: GPLv2
Group: tools
BuildRoot: %{_tmppath}/%{name}-root
Requires: libblkid
BuildRequires: pkgconfig libblkid-devel linux-headers libnih-devel
Summary: tools to manage bcache
Epoch: 5


%description
bcache tools

%install
make DESTDIR=%buildroot INSTALL=/usr/bin/install -C /bld/$RPM_PACKAGE_NAME install

%files
%_bindir/bcacheadm
%_bindir/bcachectl
%_bindir/make-bcache
%_bindir/probe-bcache
%_bindir/bcache-super-show
%_libdir/libbcache.a
%_mandir/man8/*.gz
%exclude %_prefix/etc/initramfs-tools/hooks/bcache
%exclude %_prefix/lib/udev/bcache-register
%exclude %_prefix/lib/udev/rules.d/69-bcache.rules
