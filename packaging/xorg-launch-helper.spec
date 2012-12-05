Name:		xorg-launch-helper
Version:	3
Release:	1
Summary:	Xorg service helper

Group:		System/Base
License:	GPLv2
URL:		http://foo-projects.org/~sofar/%{name}
Source0:	http://foo-projects.org/~sofar/%{name}/%{name}-%{version}.tar.gz

BuildRequires:	pkgconfig(systemd)
Requires:	/usr/bin/Xorg

%description
A wrapper to launch Xorg as a service in systemd environments.


%prep
%setup -q


%build
%configure
make %{?_smp_mflags}


%install
make install DESTDIR=$RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%{_bindir}/xorg-launch-helper
%{_libdir}/systemd/user/xorg.service
%{_libdir}/systemd/user/xorg.target
%{_libdir}/systemd/user/xorg.target.wants/xorg.service

