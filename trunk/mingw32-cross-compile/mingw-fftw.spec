%{?mingw_package_header}

Name:           mingw-fftw
Version:        3.3.3
Release:        1%{?dist}
Summary:        MinGW Windows FFTW library

License:        GPLv2+
Group:          Development/Libraries
URL:            http://www.fftw.org/
Source0:        ftp://ftp.fftw.org/pub/fftw/fftw-%{version}.tar.gz

BuildArch:      noarch

BuildRequires:  mingw32-filesystem >= 95
BuildRequires:  mingw64-filesystem >= 95
BuildRequires:  mingw32-gcc
BuildRequires:  mingw64-gcc
BuildRequires:  mingw32-binutils
BuildRequires:  mingw64-binutils


%description
FFTW is a C subroutine library for computing the Discrete Fourier
Transform (DFT) in one or more dimensions, of both real and complex
data, and of arbitrary input size.

This package contains the MinGW Windows cross-compiled FFTW library.

%package -n     mingw32-fftw
Summary:        MinGW Windows FFTW library

%description -n mingw32-fftw

FFTW is a C subroutine library for computing the Discrete Fourier
Transform (DFT) in one or more dimensions, of both real and complex
data, and of arbitrary input size.

This package contains the MinGW Windows cross-compiled FFTW library.

%package -n     mingw64-fftw
Summary:        MinGW Windows FFTW library

%description -n mingw64-fftw

FFTW is a C subroutine library for computing the Discrete Fourier
Transform (DFT) in one or more dimensions, of both real and complex
data, and of arbitrary input size.

This package contains the MinGW Windows cross-compiled FFTW library.


%{?mingw_debug_package}


%prep
%setup -q -n fftw-%{version}


%build
%{mingw_configure} \
  --disable-alloca --enable-shared --disable-static --disable-fortran \
  --with-our-malloc16 --enable-sse2
%{mingw_make} %{?_smp_mflags}


%install
%{mingw_make_install} DESTDIR=$RPM_BUILD_ROOT

# Remove .la files
rm $RPM_BUILD_ROOT%{mingw32_libdir}/*.la
rm $RPM_BUILD_ROOT%{mingw64_libdir}/*.la

# Remove documentation that duplicates what's in the native package
rm -rf ${RPM_BUILD_ROOT}%{mingw32_prefix}/share/info
rm -rf ${RPM_BUILD_ROOT}%{mingw64_prefix}/share/info
rm -rf ${RPM_BUILD_ROOT}%{mingw32_prefix}/share/man
rm -rf ${RPM_BUILD_ROOT}%{mingw64_prefix}/share/man

# We do not build the Fortran stuff
rm -f ${RPM_BUILD_ROOT}%{mingw32_includedir}/fftw3.f
rm -f ${RPM_BUILD_ROOT}%{mingw64_includedir}/fftw3.f
rm -f ${RPM_BUILD_ROOT}%{mingw32_includedir}/fftw3{l,q,}.f03
rm -f ${RPM_BUILD_ROOT}%{mingw64_includedir}/fftw3{l,q,}.f03


%files -n mingw32-fftw
%{mingw32_bindir}/libfftw3-3.dll
%{mingw32_bindir}/fftw-wisdom-to-conf
%{mingw32_bindir}/fftw-wisdom.exe
%{mingw32_includedir}/fftw3.h
%{mingw32_libdir}/libfftw3.dll.a
%{mingw32_libdir}/pkgconfig/fftw3.pc

%files -n mingw64-fftw
%{mingw64_bindir}/libfftw3-3.dll
%{mingw64_bindir}/fftw-wisdom-to-conf
%{mingw64_bindir}/fftw-wisdom.exe
%{mingw64_includedir}/fftw3.h
%{mingw64_libdir}/libfftw3.dll.a
%{mingw64_libdir}/pkgconfig/fftw3.pc


%changelog
* Mon Feb 18 2013 Yeti <yeti@gwyddion.net> - 3.3.3-1
- Updated to upstream version 3.3.3

* Sun Aug  5 2012 Yeti <yeti@gwyddion.net> - 3.3.2-2
- Duplicated all system dependences to include mingw64 variants
- Update to F17 mingw-w64 toolchain and RPM macros
- Build Win64 package
- Do not package libtool .la files
- Removed openSUSE support, don't know how to mix it with the new Fedora toolchain

* Thu Jul 12 2012 Yeti <yeti@gwyddion.net> - 3.3.2-1
- Updated to upstream version 3.3.2

* Wed Apr 11 2012 Yeti <yeti@gwyddion.net> - 3.3.1-1
- Updated to upstream version 3.3.1

* Tue Nov 29 2011 Yeti <yeti@gwyddion.net> - 3.3-1
- Updated to upstream version 3.3

* Wed Jan 26 2011 Yeti <yeti@gwyddion.net> - 3.2.2-2
- Modified to build on openSUSE

* Tue Nov 23 2010 Yeti <yeti@gwyddion.net> - 3.2.2-1
- Created
