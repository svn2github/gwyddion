%{expand:%global distro_is_redhat %(test ! -f /etc/redhat-release; echo $?)}
%{expand:%global distro_is_suse %(test ! -f /etc/SuSE-release; echo $?)}

%global __strip %{_mingw32_strip}
%global __objdump %{_mingw32_objdump}
%global _use_internal_dependency_generator 0
%global __find_requires %{_mingw32_findrequires}
%global __find_provides %{_mingw32_findprovides}
%if %{distro_is_redhat}
%define __debug_install_post %{_mingw32_debug_install_post}
%define develdep %{nil}
%endif
%if %{distro_is_suse}
%define __os_install_post %{_mingw32_debug_install_post} \
                          %{_mingw32_install_post}
%define develdep -devel
%endif

Name:           mingw32-fftw
Version:        3.3.1
Release:        1
Summary:        MinGW Windows FFTW library

License:        GPLv2+
Group:          Development/Libraries
URL:            http://www.fftw.org/
Source:         ftp://ftp.fftw.org/pub/fftw/fftw-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildArch:      noarch

BuildRequires:  mingw32-filesystem >= 52
%if %{distro_is_redhat}
BuildRequires:  mingw32-gcc
BuildRequires:  mingw32-binutils
BuildRequires:  mingw32-w32api
%endif
%if %{distro_is_suse}
BuildRequires:  mingw32-cross-gcc
BuildRequires:  mingw32-cross-binutils
BuildRequires:  mingw32-headers
%endif


%description
MinGW Windows FFTW library.

%{?_mingw32_debug_package}


%prep
%setup -q -n fftw-%{version}


%build
%{_mingw32_configure} --disable-alloca --enable-shared --disable-static --disable-fortran --with-our-malloc16 --enable-sse2
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
rm -f ${RPM_BUILD_ROOT}%{_mingw32_includedir}/fftw3{l,q,}.f03


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%{_mingw32_bindir}/libfftw3-3.dll
%{_mingw32_bindir}/fftw-wisdom-to-conf
%{_mingw32_bindir}/fftw-wisdom.exe
%{_mingw32_includedir}/fftw3.h
# Apparently Fedora leaves the .la files there.  Not sure if they are necessary.
%if %{distro_is_redhat}
%{_mingw32_libdir}/libfftw3.la
%endif
%{_mingw32_libdir}/libfftw3.dll.a
%{_mingw32_libdir}/pkgconfig/fftw3.pc


%changelog
* Wed Apr 11 2012 Yeti <yeti@gwyddion.net> - 3.3.1-1
- Updated to upstream version 3.3.1

* Tue Nov 29 2011 Yeti <yeti@gwyddion.net> - 3.3-1
- Updated to upstream version 3.3

* Wed Jan 26 2011 Yeti <yeti@gwyddion.net> - 3.2.2-2
- Modified to build on openSUSE

* Tue Nov 23 2010 Yeti <yeti@gwyddion.net> - 3.2.2-1
- Created
