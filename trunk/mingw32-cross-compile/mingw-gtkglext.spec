%{?mingw_package_header}

Name:           mingw-gtkglext
Version:        1.2.0
Release:        7
Summary:        MinGW Windows GtkGLExt library

License:        LGPLv2+
Group:          Development/Libraries
URL:            http://gtkglext.sourceforge.net/
Source0:        http://dl.sourceforge.net/sourceforge/gtkglext/gtkglext-%{version}.tar.bz2
Patch0:         gtkglext-1.2.0-nox.patch
Patch1:         gtkglext-1.2.0-nox-deprecated.patch
Patch2:         gtkglext-1.2.0-def-srcdir.patch

BuildArch:      noarch

BuildRequires:  mingw32-filesystem >= 95
BuildRequires:  mingw64-filesystem >= 95
BuildRequires:  mingw32-gcc
BuildRequires:  mingw64-gcc
BuildRequires:  mingw32-binutils
BuildRequires:  mingw64-binutils
BuildRequires:  mingw32-dlfcn
BuildRequires:  mingw64-dlfcn
BuildRequires:  mingw32-gtk2
BuildRequires:  mingw64-gtk2
BuildRequires:  mingw32-atk
BuildRequires:  mingw64-atk
BuildRequires:  mingw32-glib2
BuildRequires:  mingw64-glib2
BuildRequires:  mingw32-pkg-config
BuildRequires:  mingw64-pkg-config
# Native one for build system regeneration
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
# Native version required for glib-genmarshal
# FIXME: Is it actually used?
BuildRequires:  glib2-devel


%description
GtkGLExt is an OpenGL extension to GTK. It provides the GDK objects
which support OpenGL rendering in GTK, and GtkWidget API add-ons to
make GTK+ widgets OpenGL-capable.

This package contains the MinGW Windows cross compiled GtkGLExt library.


%package -n     mingw32-gtkglext
Summary:        MinGW Windows GtkGLExt library

%description -n mingw32-gtkglext
GtkGLExt is an OpenGL extension to GTK. It provides the GDK objects
which support OpenGL rendering in GTK, and GtkWidget API add-ons to
make GTK+ widgets OpenGL-capable.

This package contains the MinGW Windows cross compiled GtkGLExt library.


%package -n     mingw64-gtkglext
Summary:        MinGW Windows GtkGLExt library

%description -n mingw64-gtkglext
GtkGLExt is an OpenGL extension to GTK. It provides the GDK objects
which support OpenGL rendering in GTK, and GtkWidget API add-ons to
make GTK+ widgets OpenGL-capable.

This package contains the MinGW Windows cross compiled GtkGLExt library.


%{?mingw_debug_package}


%prep
%setup -q -n gtkglext-%{version}
%patch0 -p1 -b .nox
%patch1 -p1 -b .deprecated
%patch2 -p1 -b .def


%build
# patch0 patched configure.in
libtoolize --automake --force
aclocal
automake --add-missing --force
autoconf
%{mingw_configure} \
  --enable-shared --disable-static --disable-gtk-doc \
  --disable-glibtest --disable-gtktest

%{mingw_make} %{?_smp_mflags}


%install
%{mingw_make_install} DESTDIR=$RPM_BUILD_ROOT

# Remove .la files
rm $RPM_BUILD_ROOT%{mingw32_libdir}/*.la
rm $RPM_BUILD_ROOT%{mingw64_libdir}/*.la

# Remove the gtk-doc documentation and manpages which duplicate Fedora native
rm -rf $RPM_BUILD_ROOT%{mingw32_mandir}
rm -rf $RPM_BUILD_ROOT%{mingw64_mandir}
rm -rf $RPM_BUILD_ROOT%{mingw32_datadir}/gtk-doc
rm -rf $RPM_BUILD_ROOT%{mingw64_datadir}/gtk-doc


%files -n mingw32-gtkglext
%{mingw32_bindir}/libgdkglext-win32-1.0-0.dll
%{mingw32_bindir}/libgtkglext-win32-1.0-0.dll
%{mingw32_datadir}/aclocal/gtkglext-1.0.m4
%{mingw32_includedir}/gtkglext-1.0
%{mingw32_libdir}/libgdkglext-win32-1.0.dll.a
%{mingw32_libdir}/gtkglext-1.0
%{mingw32_libdir}/libgtkglext-win32-1.0.dll.a
%{mingw32_libdir}/pkgconfig/gtkglext-1.0.pc
%{mingw32_libdir}/pkgconfig/gdkglext-1.0.pc
%{mingw32_libdir}/pkgconfig/gdkglext-win32-1.0.pc
%{mingw32_libdir}/pkgconfig/gtkglext-win32-1.0.pc

%files -n mingw64-gtkglext
%{mingw64_bindir}/libgdkglext-win32-1.0-0.dll
%{mingw64_bindir}/libgtkglext-win32-1.0-0.dll
%{mingw64_datadir}/aclocal/gtkglext-1.0.m4
%{mingw64_includedir}/gtkglext-1.0
%{mingw64_libdir}/libgdkglext-win32-1.0.dll.a
%{mingw64_libdir}/gtkglext-1.0
%{mingw64_libdir}/libgtkglext-win32-1.0.dll.a
%{mingw64_libdir}/pkgconfig/gtkglext-1.0.pc
%{mingw64_libdir}/pkgconfig/gdkglext-1.0.pc
%{mingw64_libdir}/pkgconfig/gdkglext-win32-1.0.pc
%{mingw64_libdir}/pkgconfig/gtkglext-win32-1.0.pc


%changelog
* Mon Aug  6 2012 Yeti <yeti@gwyddion.net> - 1.2.0-7
- Duplicated all system dependences to include mingw64 variants
- Update to F17 mingw-w64 toolchain and RPM macros
- Build Win64 package
- Added explict cross-pkg-config dependencies, seem needed
- Do not package libtool .la files
- Fix paths to *.def files in Makefiles to build with builddir != srcdir
- Removed openSUSE support, don't know how to mix it with the new Fedora toolchain

* Tue Nov 29 2011 Yeti <yeti@gwyddion.net> - 1.2.0-6
- Corrected inverted gtk_widget_get_has_window() assertion in depecated patch.

* Wed Nov 23 2011 Yeti <yeti@gwyddion.net> - 1.2.0-5
- Corrected compatibility of nox and deprecated patches.
- Rebuilt to make it work with new Win32 iconv packages on F16

* Thu Jan 27 2011 Yeti <yeti@gwyddion.net> - 1.2.0-4
- BuilRequire -devel packages on openSUSE

* Wed Jan 26 2011 Yeti <yeti@gwyddion.net> - 1.2.0-3
- Modified to build on openSUSE

* Tue Jan 25 2011 Yeti <yeti@gwyddion.net> - 1.2.0-2
- Updated to build with modern Gtk+
- Corrected gtkdoc typo

* Tue Nov 23 2010 Yeti <yeti@gwyddion.net> - 1.2.0-1
- Created
