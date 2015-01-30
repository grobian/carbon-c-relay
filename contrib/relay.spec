Name:		relay
Version:	0.32
Release:	1%{?dist}
Summary:	A C implementation of the Graphite carbon relay daemon packaged for mdr.
Group:		System Environment/Daemons
License:	See the LICENSE file at github.
URL:		https://github.com/grobian/carbon-c-relay
Source0:	%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-root
BuildRequires:  openssl-devel
Requires(pre):  /usr/sbin/useradd
Requires:       daemonize
AutoReqProv:	No

%description

Carbon-like graphite line mode relay.
This project aims to be a replacement of the original Carbon relay.
Carbon C Relay has been packed as part of twiki for the BBC.

%prep
%setup -q

%build
make %{?_smp_mflags}

%install
mkdir -vp $RPM_BUILD_ROOT/var/log/carbon/
mkdir -vp $RPM_BUILD_ROOT/etc/monit.d/
mkdir -vp $RPM_BUILD_ROOT/var/run/carbon
mkdir -vp $RPM_BUILD_ROOT/var/lib/carbon
mkdir -vp $RPM_BUILD_ROOT/usr/bin
mkdir -vp $RPM_BUILD_ROOT/etc/init.d
mkdir -vp $RPM_BUILD_ROOT/etc/sysconfig
install -m 755 relay $RPM_BUILD_ROOT/usr/bin/relay
install -m 644 contrib/relay.conf $RPM_BUILD_ROOT/etc/relay.conf
install -m 755 contrib/relay.init $RPM_BUILD_ROOT/etc/init.d/relay
install -m 644 contrib/relay.sysconfig $RPM_BUILD_ROOT/etc/sysconfig/relay
install -m 644 contrib/relay.monit $RPM_BUILD_ROOT/etc/monit.d/relay.conf

%clean
make clean

%pre
getent group carbon >/dev/null || groupadd -r carbon
getent passwd carbon >/dev/null || \
  useradd -r -g carbon -s /sbin/nologin \
    -d $RPM_BUILD_ROOT/var/lib/carbon/ -c "Carbon Daemons" carbon
exit 0

%post
chgrp carbon /var/run/carbon
chmod 774 /var/run/relay
chown carbon:carbon /var/log/carbon
chmod 744 /var/log/carbon

%files
%defattr(-,root,root,-)
/usr/bin/relay
%config(noreplace) /etc/relay.conf
/etc/init.d/relay
%config(noreplace) /etc/sysconfig/relay
%config(noreplace) /etc/monit.d/relay.conf
#/var/run/carbon
#/var/log/carbon

%changelog
* Mon Sep 8 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- tidy up for github
- reverted site specific changes

* Tue Aug 8 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- packaged as part of twiki

* Tue Jul 1 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- packaged as part of mdr
- binary renamed from 'relay' to 'cc_relay'
- pagage renamed to reflect function rather than component
- user / group named by function

* Tue May 6 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- Initial package for the BBC
