%global __strip %{_mingw32_strip}
%global __objdump %{_mingw32_objdump}
%global _use_internal_dependency_generator 0
%global __find_requires %{_mingw32_findrequires}
%global __find_provides %{_mingw32_findprovides}
%define __debug_install_post %{_mingw32_debug_install_post}

Name:           mingw32-gtkglext
Version:        1.2.0
Release:        2%{?dist}
Summary:        MinGW Windows GtkGLExt library

License:        LGPLv2+
Group:          Development/Libraries
URL:            http://gtkglext.sourceforge.net/
Source0:        http://dl.sourceforge.net/sourceforge/gtkglext/gtkglext-%{version}.tar.bz2
Patch0:         gtkglext-1.2.0-nox.patch
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildArch:      noarch

BuildRequires:  mingw32-filesystem >= 52
BuildRequires:  mingw32-gcc
BuildRequires:  mingw32-binutils
BuildRequires:  mingw32-dlcfn
BuildRequires:  mingw32-gtk2
BuildRequires:  mingw32-glib2
BuildRequires:  mingw32-w32api

BuildRequires:  pkgconfig
# Native version required for glib-genmarshal
# FIXME: Is it actually used?
BuildRequires:  glib2-devel

Requires:       pkgconfig


%description
MinGW Windows GtkGLExt library.

%{?_mingw32_debug_package}


%prep
%setup -q -n gtkglext-%{version}
%patch0 -p1 -b .nox


%build
# patch0 patched configure.in
libtoolize --automake --force
aclocal -I m4
automake --add-missing --force
autoconf
%{_mingw32_configure} --enable-shared --disable-static --disable-gtkdoc --disable-glibtest --disable-gtktest
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
%{_mingw32_makeinstall}

# Remove the gtk-doc documentation and manpages which duplicate Fedora native
rm -rf $RPM_BUILD_ROOT%{_mingw32_mandir}
rm -rf $RPM_BUILD_ROOT%{_mingw32_datadir}/gtk-doc


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)

%{_mingw32_bindir}/libgdkglext-win32-1.0-0.dll
%{_mingw32_bindir}/libgtkglext-win32-1.0-0.dll
%{_mingw32_datadir}/aclocal/gtkglext-1.0.m4
%{_mingw32_includedir}/gtkglext-1.0
%{_mingw32_libdir}/libgdkglext-win32-1.0.dll.a
%{_mingw32_libdir}/gtkglext-1.0
%{_mingw32_libdir}/libgtkglext-win32-1.0.dll.a
%{_mingw32_libdir}/libgdkglext-win32-1.0.la
%{_mingw32_libdir}/libgtkglext-win32-1.0.la
%{_mingw32_libdir}/pkgconfig/gtkglext-1.0.pc
%{_mingw32_libdir}/pkgconfig/gdkglext-1.0.pc
%{_mingw32_libdir}/pkgconfig/gdkglext-win32-1.0.pc
%{_mingw32_libdir}/pkgconfig/gtkglext-win32-1.0.pc


%changelog
* Tue Nov 23 2010 Yeti <yeti@gwyddion.net> - 1.2.0-1
- Created
