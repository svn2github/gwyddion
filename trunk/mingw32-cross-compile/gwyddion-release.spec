Name:           gwyddion-release
Version:        17
Release:        1%{?dist}
Summary:        Gwyddion Fedora Repository Configuration

Group:          System Environment/Base
License:        BSD
URL:            http://gwyddion.net/
Source1:        gwyddion.repo
Source2:        RPM-GPG-KEY-gwyddion
BuildArch:      noarch

Requires:       system-release >= 17

%description
Gwyddion RPM repository contains Gwyddion and extra MinGW packages used for
cross-compilation of Gwyddion for MS Windows.


%prep
echo "Nothing to prep"


%build
echo "Nothing to build"


%install
mkdir -p -m 755 \
  $RPM_BUILD_ROOT%{_sysconfdir}/pki/rpm-gpg  \
  $RPM_BUILD_ROOT%{_sysconfdir}/yum.repos.d

%{__install} -p -m 644 %{SOURCE1} $RPM_BUILD_ROOT%{_sysconfdir}/yum.repos.d
%{__install} -p -m 644 %{SOURCE2} $RPM_BUILD_ROOT%{_sysconfdir}/pki/rpm-gpg


%files
%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-gwyddion
%config(noreplace) %{_sysconfdir}/yum.repos.d/gwyddion.repo

%changelog
* Sun Aug  5 2012 Yeti <yeti@gwyddion.net> - 17-1
- Created
