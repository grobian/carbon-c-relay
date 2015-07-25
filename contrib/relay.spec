Name:           carbon-c-relay
Version:        0.42
Release:        1%{?dist}
Summary:        A C implementation of the Graphite carbon relay daemon packaged for MDR
Group:          System Environment/Daemons
License:        See the LICENSE file at github.
URL:            https://github.com/grobian/carbon-c-relay
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-root
BuildRequires:  openssl-devel
Requires(pre):  /usr/sbin/useradd
Requires:       daemonize
Requires(post): chkconfig
AutoReqProv:    No

%description

Carbon-like graphite line mode relay.
This project aims to be a replacement of the original Carbon relay.
Carbon C Relay has been packed as part of twiki for the BBC.

%prep
%setup -q -n %{name}-%{version}
%{__sed} -i s/%%%%NAME%%%%/%{name}/g contrib/relay.init
%{__sed} -i s/%%%%NAME%%%%/%{name}/g contrib/relay.logrotate
%{__sed} -i s/%%%%NAME%%%%/%{name}/g contrib/relay.monit
%{__sed} -i s/%%%%NAME%%%%/%{name}/g contrib/relay.sysconfig

%build
make %{?_smp_mflags}

%install
mkdir -vp %{buildroot}%{_localstatedir}/log/%{name}
mkdir -vp %{buildroot}%{_sysconfdir}/monit.d/
mkdir -vp %{buildroot}%{_sysconfdir}/logrotate.d/
mkdir -vp %{buildroot}%{_localstatedir}/run/%{name}
mkdir -vp %{buildroot}%{_localstatedir}/lib/%{name}
mkdir -vp %{buildroot}%{_bindir}
mkdir -vp %{buildroot}%{_sysconfdir}/init.d
mkdir -vp %{buildroot}%{_sysconfdir}/sysconfig
%{__install} -m 755 relay %{buildroot}%{_bindir}/%{name}
%{__install} -m 644 contrib/relay.conf %{buildroot}%{_sysconfdir}/%{name}.conf
%{__install} -m 755 contrib/relay.init %{buildroot}%{_sysconfdir}/init.d/%{name}
%{__install} -m 644 contrib/relay.logrotate %{buildroot}%{_sysconfdir}/logrotate.d/%{name}
%{__install} -m 644 contrib/relay.monit %{buildroot}%{_sysconfdir}/monit.d/%{name}.conf
%{__install} -m 644 contrib/relay.sysconfig %{buildroot}%{_sysconfdir}/sysconfig/%{name}

%clean
make clean

%pre
getent group %{name} >/dev/null || groupadd -r %{name}
getent passwd %{name} >/dev/null || \
  useradd -r -g %{name} -s /sbin/nologin \
    -d %{_localstatedir}/lib/%{name}/ -c "Carbon C Relay Daemon" %{name}
exit 0

%post
chgrp %{name} %{_localstatedir}/run/%{name}
chmod 774 %{_localstatedir}/run/%{name}
chown %{name}:%{name} %{_localstatedir}/log/%{name}
chmod 744 %{_localstatedir}/log/%{name}
/sbin/chkconfig --add %{name}

%preun
/sbin/chkconfig --del %{name}


%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_sysconfdir}/init.d/%{name}
%config(noreplace) %{_sysconfdir}/%{name}.conf
%config(noreplace) %{_sysconfdir}/sysconfig/%{name}
%config(noreplace) %{_sysconfdir}/logrotate.d/%{name}
%config(noreplace) %{_sysconfdir}/monit.d/%{name}.conf

%defattr(655, %{name}, %{name}, 755)
%{_localstatedir}/run/%{name}
%{_localstatedir}/log/%{name}



%changelog
* Tue Mar 24 2015 Andy "Bob" Brockhurst <andy.brockhurst@b3cft.com>
- updated spec file to use rpm macros where possible
- updated %setup to use carbon-c-relay as tar extracted location
- changed user/group to %{name}
- added placeholder %%NAME%% in init, logrotate, monit, sysconfig
-- added sed command to replace %%NAME%% in above at prep stage
- added creation of /var/run/%{name} and /var/log/%{name}
- added chkconfig --add to %post
- added logrotate file to spec
- bump to version 0.39

* Mon Sep 8 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- tidy up for github
- reverted site specific changes

* Fri Aug 8 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- packaged as part of twiki

* Tue Jul 1 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- packaged as part of mdr
- binary renamed from 'relay' to '%{name}'
- pagage renamed to reflect function rather than component
- user / group named by function

* Tue May 6 2014 Matthew Hollick <matthew@mayan-it.co.uk>
- Initial package for the BBC
