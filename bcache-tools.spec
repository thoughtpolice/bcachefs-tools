Summary: bcache-tools
Name: bcache-tools
Version: 0.1
Release: %{?release:%{release}}%{!?release:eng}
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: tools
BuildRoot: %{_tmppath}/%{name}-root
Requires: libblkid
BuildRequires: pkgconfig libblkid-devel linux-headers libnih-devel

%package dev

%description

%files
