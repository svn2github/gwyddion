%global __strip %{_mingw32_strip}
%global __objdump %{_mingw32_objdump}
%global _use_internal_dependency_generator 0
%global __find_requires %{_mingw32_findrequires}
%global __find_provides %{_mingw32_findprovides}
%define __debug_install_post %{_mingw32_debug_install_post}

Name:           mingw32-fftw
Version:        3.2.2
Release:        1%{?dist}
Summary:        MinGW Windows FFTW library

License:        GPLv2+
Group:          Development/Libraries
URL:            http://www.fftw.org/
Source:         ftp://ftp.fftw.org/pub/fftw/fftw-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildArch:      noarch

BuildRequires:  mingw32-filesystem >= 52
BuildRequires:  mingw32-gcc
BuildRequires:  mingw32-binutils
BuildRequires:  mingw32-w32api


%description
MinGW Windows FFTW library.

%{?_mingw32_debug_package}


%prep
%setup -q -n fftw-%{version}


%build
%{_mingw32_configure} --enable-shared --disable-static --disable-fortran --with-our-malloc16 --enable-sse2
make %{?_smp_mflags}

#==========================================
%install
rm -rf ${RPM_BUILD_ROOT}
%{_mingw32_makeinstall}

# Remove the info documentation and manpages which duplicate Fedora native
rm -rf ${RPM_BUILD_ROOT}%{_mingw32_prefix}/share/info
rm -rf ${RPM_BUILD_ROOT}%{_mingw32_prefix}/share/man
# We do not build the Fortran stuff
rm -f ${RPM_BUILD_ROOT}%{_mingw32_includedir}/fftw3.f


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%{_mingw32_bindir}/libfftw3-3.dll
%{_mingw32_bindir}/fftw-wisdom-to-conf
%{_mingw32_bindir}/fftw-wisdom.exe
%{_mingw32_includedir}/fftw3.h
%{_mingw32_libdir}/libfftw3.la
%{_mingw32_libdir}/libfftw3.dll.a
%{_mingw32_libdir}/pkgconfig/fftw3.pc


%changelog
* Tue Nov 23 2010 Yeti <yeti@gwyddion.net> - 3.2.2-1
- Created
