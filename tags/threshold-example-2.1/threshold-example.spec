%define modname threshold-example

Summary: An example Gwyddion threshold module
Name: gwyddion-%{modname}
Version: 2.0
Release: 1
License: GNU GPL
Group: Applications/Engineering
Source: http://gwyddion.net/download/modules/%{modname}-2.0.tar.bz2
URL: http://gwyddion.net/
Prefix: %{_prefix}
BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot
Requires: gwyddion
BuildPrereq: gtk2-devel
BuildPrereq: gwyddion-devel
BuildPrereq: libtool
BuildPrereq: pkgconfig

%description
Gwyddion is a modular SPM (scanning probe microscope) data analysis framework.
It uses Gtk+.

This package contains a standalone threshold module, mostly useful as
a standalone module example.

%prep
%setup -q -n %{modname}-%{version}

%build
%__make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && %__rm -rf $RPM_BUILD_ROOT
%makeinstall DESTDIR=$RPM_BUILD_ROOT
%__strip $RPM_BUILD_ROOT%{_libdir}/gwyddion/modules/process/%{modname}.so

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && %__rm -rf $RPM_BUILD_ROOT

%files
%defattr(755,root,root)
%{_libdir}/gwyddion/modules/process/%{modname}.so
%dir %{_libdir}/gwyddion/modules/process
%dir %{_libdir}/gwyddion/modules
%dir %{_libdir}/gwyddion
%defattr(-,root,root)
%doc COPYING README
