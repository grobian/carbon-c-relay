Name:		cc_relay
Version:	0.30
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
#Requires:       mdr_agent-monit
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
mkdir -vp $RPM_BUILD_ROOT/var/log/cc_relay/
install -m 755 relay $RPM_BUILD_ROOT/usr/bin/cc_relay
install -m 644 contrib/relay.conf $RPM_BUILD_ROOT/etc/cc_relay.conf
install -m 755 contrib/relay.init $RPM_BUILD_ROOT/etc/init.d/cc_relay
install -m 644 contrib/relay.sysconfig $RPM_BUILD_ROOT/etc/sysconfig/cc_relay
mkdir -vp $RPM_BUILD_ROOT/var/run/cc_relay

%clean
make clean

%pre
getent group mdr_relay >/dev/null || groupadd -r mdr_relay
getent passwd mdr_relay >/dev/null || \
  useradd -r -g mdr_relay -s /sbin/nologin \
    -d $RPM_BUILD_ROOT/var/log/cc_relay/ -c "Carbon C Relay for mdr" mdr_relay
exit 0

%post
chgrp mdr_relay /var/run/cc_relay
chmod 774 /var/run/cc_relay
chown mdr_relay:mdr_relay /var/log/cc_relay
chmod 744 /var/log/cc_relay

%files
%defattr(-,root,root,-)
/usr/bin/cc_relay
/etc/cc_relay.conf
/etc/init.d/cc_relay
/etc/sysconfig/cc_relay
/var/run/cc_relay
/var/log/cc_relay

%changelog
* Tue Aug 8 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- packaged as part of twiki

%changelog
* Tue Jul 1 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- packaged as part of mdr
- binary renamed from 'relay' to 'cc_relay'
- pagage renamed to reflect function rather than component
- user / group named by function

* Tue May 6 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- Initial package for the BBC
