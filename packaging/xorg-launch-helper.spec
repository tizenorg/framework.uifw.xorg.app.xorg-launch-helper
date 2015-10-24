Name:		xorg-launch-helper
Version:	4.0.3
Release:	4
Summary:	Xorg service helper

Group:		Graphics/X Window System
License:	GPL-2.0
URL:		http://foo-projects.org/~sofar/%{name}
Source0:	http://foo-projects.org/~sofar/%{name}/%{name}-%{version}.tar.gz
Source1:	xorg.conf
Source2:	xorg_done.service
Source1001: 	xorg-launch-helper.manifest

BuildRequires:	pkgconfig(systemd)
BuildRequires:	pkgconfig(libsystemd-daemon)
BuildRequires:  pkgconfig(ttrace)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(glib-2.0)
Requires:	/usr/bin/Xorg

%description
A wrapper to launch Xorg as a service in systemd environments.

%prep
%setup -q
cp %{SOURCE1001} .

%build
%reconfigure --with-systemduserunitdir="%{_prefix}/lib/systemd/system"
make %{?_smp_mflags}


%install
mkdir -p %{buildroot}/usr/share/license
cp -af COPYING %{buildroot}/usr/share/license/%{name}
%make_install

# temoprary HW configuration. it should be seperated.
mkdir -p %{buildroot}/etc/sysconfig
mkdir -p %{buildroot}%{_prefix}/lib/systemd/system/
install -m 644 %{SOURCE1} %{buildroot}/etc/sysconfig/xorg
install -m 0644 %{SOURCE2} %{buildroot}%{_prefix}/lib/systemd/system/
ln -sf ../xorg_done.service %{buildroot}%{_prefix}/lib/systemd/system/basic.target.wants

# "-sharevt" option will be removed (only) for Tizen Emulator temporarily
# by the request from Tizen SDK (kernel) team.
%if 0%{?simulator}
sed -i 's/-sharevts//g' %{buildroot}/etc/sysconfig/xorg
%endif

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
/usr/share/license/%{name}
%{_bindir}/xorg-launch-helper
%{_prefix}/lib/systemd/system/xorg.service
%{_prefix}/lib/systemd/system/xorg_done.service
%{_prefix}/lib/systemd/system/xorg.target
%{_prefix}/lib/systemd/system/basic.target.wants/xorg.service
%{_prefix}/lib/systemd/system/basic.target.wants/xorg_done.service
/etc/sysconfig/xorg
