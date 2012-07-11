%global __strip %{_mingw32_strip}
%global __objdump %{_mingw32_objdump}
%define __debug_install_post %{_mingw32_debug_install_post}

%define release_version %(echo %{version} | awk -F. '{print $1"."$2}')

%define po_package gtksourceview-2.0

Name:           mingw32-gtksourceview2
Version:        2.11.2
Release:        1%{?dist}
Summary:        MinGW Windows library for viewing source files

# the library itself is LGPL, some .lang files are GPL
License:        LGPLv2+ and GPLv2+
Group:          Development/Libraries
URL:            http://www.gtk.org
Source0:        http://download.gnome.org/sources/gtksourceview/%{release_version}/gtksourceview-%{version}.tar.bz2

BuildArch:      noarch

BuildRequires:  mingw32-filesystem >= 68
BuildRequires:  mingw32-gcc
BuildRequires:  mingw32-binutils
BuildRequires:  mingw32-gettext
BuildRequires:  mingw32-gtk2
BuildRequires:  mingw32-libxml2

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

%{?_mingw32_debug_package}


%prep
%setup -q -n gtksourceview-%{version}


%build
sed -i -e 's/\<G_CONST_RETURN\>/const/g' \
       -e 's/\<G_UNICODE_COMBINING_MARK\>/G_UNICODE_SPACING_MARK/g' \
       -e 's/\<gtk_set_locale\>/setlocale/g' \
       gtksourceview/*.[ch]
%{_mingw32_configure} \
  --disable-static \
  --disable-gtk-doc \
  --disable-introspection

make %{?_smp_mflags} V=1


%install
rm -rf $RPM_BUILD_ROOT
%{_mingw32_makeinstall}

# remove unwanted files
rm $RPM_BUILD_ROOT%{_mingw32_datadir}/gtksourceview-2.0/language-specs/check-language.sh
rm $RPM_BUILD_ROOT%{_mingw32_datadir}/gtksourceview-2.0/styles/check-style.sh

# Remove documentation that duplicates what's in the native package
rm -rf $RPM_BUILD_ROOT%{_mingw32_datadir}/gtk-doc

%find_lang %{po_package}


%files -f %{po_package}.lang
%doc COPYING
%{_mingw32_bindir}/libgtksourceview-2.0-0.dll
%{_mingw32_includedir}/gtksourceview-2.0/
%{_mingw32_libdir}/libgtksourceview-2.0.dll.a
%{_mingw32_libdir}/libgtksourceview-2.0.la
%{_mingw32_libdir}/pkgconfig/gtksourceview-2.0.pc
%{_mingw32_datadir}/gtksourceview-2.0/


%changelog
* Thu Jul 11 2012 Yeti <yeti@gwyddion.net> - 2.11.2-1
- Initial release

