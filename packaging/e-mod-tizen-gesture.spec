%bcond_with x
%bcond_with wayland

Name: e-mod-tizen-gesture
Version: 0.2.8
Release: 1
Summary: The Enlightenment Gesture Module for Tizen
URL: http://www.enlightenment.org
Group: Graphics & UI Framework/Other
Source0: %{name}-%{version}.tar.gz
License: BSD-2-Clause
BuildRequires:  pkgconfig(enlightenment)
BuildRequires:  gettext
BuildRequires:  pkgconfig(wayland-server)
BuildRequires:  pkgconfig(tizen-extension-server)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(tzsh-server)

%global TZ_SYS_RO_SHARE  %{?TZ_SYS_RO_SHARE:%TZ_SYS_RO_SHARE}%{!?TZ_SYS_RO_SHARE:/usr/share}

%description
This package is a the Enlightenment Gesture Module for Tizen.

%prep
%setup -q

%build

export GC_SECTIONS_FLAGS="-fdata-sections -ffunction-sections -Wl,--gc-sections"
export CFLAGS+=" -Wall -g -fPIC -rdynamic ${GC_SECTIONS_FLAGS} -DE_LOGGING=1 "
export LDFLAGS+=" -Wl,--hash-style=both -Wl,--as-needed -Wl,--rpath=/usr/lib"

%autogen
%configure --prefix=/usr \
           --enable-wayland-only \
make

%install
rm -rf %{buildroot}

# install
make install DESTDIR=%{buildroot}

# clear useless textual files
find  %{buildroot}%{_libdir}/enlightenment/modules/%{name} -name *.la | xargs rm

%files
%defattr(-,root,root,-)
%license COPYING
%{_libdir}/enlightenment/modules/e-mod-tizen-gesture
