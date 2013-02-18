%{?mingw_package_header}
%define release_version %(echo %{version} | awk -F. '{print $1"."$2}')
%define po_package gtksourceview-2.0

Name:           mingw-gtksourceview2
Version:        2.11.2
Release:        4%{?dist}
Summary:        MinGW Windows library for viewing source files

# the library itself is LGPL, some .lang files are GPL
License:        LGPLv2+ and GPLv2+
Group:          Development/Libraries
URL:            http://www.gtk.org
Source0:        http://download.gnome.org/sources/gtksourceview/%{release_version}/gtksourceview-%{version}.tar.bz2
Patch0:         mingw-libtool-win64-lib.patch

BuildArch:      noarch

BuildRequires:  mingw32-filesystem >= 95
BuildRequires:  mingw64-filesystem >= 95
BuildRequires:  mingw32-gcc
BuildRequires:  mingw64-gcc
BuildRequires:  mingw32-binutils
BuildRequires:  mingw64-binutils
BuildRequires:  mingw32-gettext
BuildRequires:  mingw64-gettext
BuildRequires:  mingw32-gtk2
BuildRequires:  mingw64-gtk2
BuildRequires:  mingw32-libxml2
BuildRequires:  mingw64-libxml2
BuildRequires:  mingw32-pkg-config
BuildRequires:  mingw64-pkg-config

# Native one for msgfmt
BuildRequires:  gettext
# Native one for glib-genmarshal and glib-mkenums
BuildRequires:  glib2-devel
BuildRequires:  intltool

%description
GtkSourceView is a text widget that extends the standard GTK+
GtkTextView widget. It improves GtkTextView by implementing
syntax highlighting and other features typical of a source code editor.

This package contains the MinGW Windows cross compiled GtkSourceView library,
version 2.


%package -n     mingw32-gtksourceview2
Summary:        MinGW Windows library for viewing source files

%description -n mingw32-gtksourceview2
GtkSourceView is a text widget that extends the standard GTK+
GtkTextView widget. It improves GtkTextView by implementing
syntax highlighting and other features typical of a source code editor.

This package contains the MinGW Windows cross compiled GtkSourceView library,
version 2.


%package -n     mingw64-gtksourceview2
Summary:        MinGW Windows library for viewing source files

%description -n mingw64-gtksourceview2
GtkSourceView is a text widget that extends the standard GTK+
GtkTextView widget. It improves GtkTextView by implementing
syntax highlighting and other features typical of a source code editor.

This package contains the MinGW Windows cross compiled GtkSourceView library,
version 2.


%{?mingw_debug_package}


%prep
%setup -q -n gtksourceview-%{version}
%patch0 -p1 -b .win64
sed -i -e 's/\<G_CONST_RETURN\>/const/g' \
       -e 's/\<G_UNICODE_COMBINING_MARK\>/G_UNICODE_SPACING_MARK/g' \
       -e 's/\<gtk_set_locale\>/setlocale/g' \
       gtksourceview/*.[ch]


%build
%{mingw_configure} \
  --disable-static \
  --disable-gtk-doc \
  --disable-introspection

%{mingw_make} %{?_smp_mflags}


%install
%{mingw_make_install} DESTDIR=$RPM_BUILD_ROOT

# remove unwanted files
rm $RPM_BUILD_ROOT%{mingw32_datadir}/gtksourceview-2.0/language-specs/check-language.sh
rm $RPM_BUILD_ROOT%{mingw64_datadir}/gtksourceview-2.0/language-specs/check-language.sh
rm $RPM_BUILD_ROOT%{mingw32_datadir}/gtksourceview-2.0/styles/check-style.sh
rm $RPM_BUILD_ROOT%{mingw64_datadir}/gtksourceview-2.0/styles/check-style.sh

# Remove .la files
rm $RPM_BUILD_ROOT%{mingw32_libdir}/*.la
rm $RPM_BUILD_ROOT%{mingw64_libdir}/*.la

# Remove documentation that duplicates what's in the native package
rm -rf $RPM_BUILD_ROOT%{mingw32_datadir}/gtk-doc
rm -rf $RPM_BUILD_ROOT%{mingw64_datadir}/gtk-doc

%{mingw_find_lang} %{po_package}



%files -n mingw32-gtksourceview2 -f mingw32-%{po_package}.lang
%doc COPYING
%{mingw32_bindir}/libgtksourceview-2.0-0.dll
%{mingw32_includedir}/gtksourceview-2.0/
%{mingw32_libdir}/libgtksourceview-2.0.dll.a
%{mingw32_libdir}/pkgconfig/gtksourceview-2.0.pc
%{mingw32_datadir}/gtksourceview-2.0/


%files -n mingw64-gtksourceview2 -f mingw64-%{po_package}.lang
%doc COPYING
%{mingw64_bindir}/libgtksourceview-2.0-0.dll
%{mingw64_includedir}/gtksourceview-2.0/
%{mingw64_libdir}/libgtksourceview-2.0.dll.a
%{mingw64_libdir}/pkgconfig/gtksourceview-2.0.pc
%{mingw64_datadir}/gtksourceview-2.0/



%changelog
* Mon Feb 18 2013 Yeti <yeti@gwyddion.net> - 2.11.2-4
- Rebuilt for F18

* Sat Aug  4 2012 Yeti <yeti@gwyddion.net> - 2.11.2-3
- Duplicated all system dependences to include mingw64 variants
- Made build quiet

* Wed Aug  1 2012 Yeti <yeti@gwyddion.net> - 2.11.2-2
- Update to F17 mingw-w64 toolchain and RPM macros
- Build Win64 package
- Added explict cross-pkg-config dependencies, seem needed
- Do not package libtool .la files

* Thu Jul 11 2012 Yeti <yeti@gwyddion.net> - 2.11.2-1
- Initial release

