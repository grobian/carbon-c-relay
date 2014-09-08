Name:		carbon-c-relay
Version:	0.29
Release:	1%{?dist}
Summary:	A C implementation of the Graphite carbon relay daemon.
Group:		System Environment/Daemons
License:	See the LICENSE file.
URL:		https://github.com/grobian/carbon-c-relay
Source0:	%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-root
BuildRequires:  openssl-devel
Requires(pre):  /usr/sbin/useradd
Requires(post): chkconfig
Requires:       daemonize
AutoReqProv:	No

%description

Carbon-like graphite line mode relay.
This project aims to be a replacement of the original Carbon relay.

%prep
%setup -q

%build
make %{?_smp_mflags}

%install
mkdir -vp $RPM_BUILD_ROOT/usr/bin
mkdir -vp $RPM_BUILD_ROOT/etc/init.d
mkdir -vp $RPM_BUILD_ROOT/etc/sysconfig/
mkdir -vp $RPM_BUILD_ROOT/var/log/relay/
install -m 755 relay $RPM_BUILD_ROOT/usr/bin/relay
install -m 644 contrib/relay.conf $RPM_BUILD_ROOT/etc/
install -m 755 contrib/relay.init $RPM_BUILD_ROOT/etc/init.d/relay
install -m 644 contrib/relay.sysconfig $RPM_BUILD_ROOT/etc/sysconfig/relay
mkdir -vp $RPM_BUILD_ROOT/var/run/relay

%clean
make clean

%pre
getent group relay >/dev/null || groupadd -r relay
getent passwd relay >/dev/null || \
  useradd -r -g relay -s /sbin/nologin \
    -d $RPM_BUILD_ROOT/usr/libexec/relay -c "Carbon C Relay" relay
exit 0

%post
chgrp relay /var/run/relay
chmod 774 /var/run/relay
chown relay:relay /var/log/relay
chmod 744 /var/log/relay
/sbin/chkconfig --add relay

%files
%defattr(-,root,root,-)
%doc LICENSE.md
%doc README.md
/usr/bin/relay
/etc/relay.conf
/etc/init.d/relay
/etc/sysconfig/relay
/var/run/relay
/var/log/relay

%changelog
* Tue May 6 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- Initial package for the BBC
