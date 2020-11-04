#
# Supported build options
#   --without pgsql     Disables pgsql detection
#   --without mysql     Disables mysql detection
#   --without java      Disables java detection
#   --with autoconf     Runautoconf to generate the configure script
#   --with fast_maildir Enables -DFAST_MAILDIR 
#   --with sort_maildir Enables -DSORT_MAILDIR 
#
Name: teapop
# When changing to new version, remember to reset Release to 1
Version: 0.3.8
Release: 1
License: BSD
Summary: TEAPOP POP Server
Group: System Environment/Daemons
Packager: Ivan F. Martinez <ivanfm@ecodigit.com.br>
Source: teapop-%{version}.tar.gz
URL: http://www.toontown.org/teapop/
BuildRoot: %{_tmppath}/%{name}-%{version}-root
%description
Modular POP server.

%prep

%setup

%build
EXTRAS=

if [ -f /usr/include/pgsql/postgres.h ] || \
   [ -f /usr/include/pgsql/server/postgres.h ]
then
    EXTRAS="$EXTRAS %{!?_without_pgsql: --with-pgsql=/usr} "
fi

if [ -f /usr/include/mysql.h ] || \
   [ -f /usr/include/mysql/mysql.h ]
then
    EXTRAS="$EXTRAS %{!?_without_mysql: --with-mysql=/usr} "
fi

if [ "$JAVA_HOME" != "" ] && [ ! -d "$JAVA_HOME" ]
then
    JAVA_HOME=""
fi
for dir in /opt/IBMJava2-*
do
    if [ -d "$dir" ]
    then
        JAVA_HOME="$dir"
    fi
done

if [ "$JAVA_HOME" != "" ]
then
    EXTRAS="$EXTRAS %{!?_without_java:--with-java=$JAVA_HOME}"
    if [ "$JAVA_AUTH_CLASS" != "" ]
    then
        EXTRAS="$EXTRAS --with-javaclass=$JAVA_AUTH_CLASS"
    fi
fi

export CFLAGS="$CFLAGS %{?_with_fast_maildir: -DFAST_MAILDIR}"
export CFLAGS="$CFLAGS %{?_with_sort_maildir: -DSORT_MAILDIR}"
%{?_with_autoconf:cd config ; chmod +w configure ; autoconf ; cd ..}
./configure ${EXTRAS} \
     --enable-lock=flock \
     --prefix="" \
     --mandir="$RPM_BUILD_ROOT/usr/share/man" \
     --libexecdir="$RPM_BUILD_ROOT/usr/sbin"
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/etc/xinetd.d
mkdir -p $RPM_BUILD_ROOT/etc/stunnel
cp contrib/rpm/teapop.xinetd  $RPM_BUILD_ROOT/etc/xinetd.d/teapop_xinetd
cp contrib/rpm/teapop.stunnel $RPM_BUILD_ROOT/etc/xinetd.d/teapop_stunnel
cp contrib/rpm/teapop.stunnel4 $RPM_BUILD_ROOT/etc/xinetd.d/teapop_stunnel4
cp contrib/rpm/teapop.stunnel4.conf $RPM_BUILD_ROOT/etc/stunnel/teapop.conf
mkdir -p $RPM_BUILD_ROOT/etc/init.d
cp contrib/rpm/teapop.init.d $RPM_BUILD_ROOT/etc/init.d/teapop
make prefix=$RPM_BUILD_ROOT install

%files
%defattr(-,root,root)                                                                                         
%config(noreplace) /etc/teapop.passwd
%config(noreplace) /etc/xinetd.d/teapop_xinetd
%config(noreplace) /etc/xinetd.d/teapop_stunnel
%config(noreplace) /etc/xinetd.d/teapop_stunnel4
%config(noreplace) /etc/stunnel/teapop.conf
%doc doc/*
%doc contrib
/etc/init.d/teapop
/usr/*

%changelog
* Thu Jun 19 2003 Ivan F. Martinez <ivanfm@users.sourceforge.net>
- Accept predefined JAVA_HOME environment to specify where java is installed
- Option to generate autoconf to update configure script
- Accept Authentication class in environment JAVA_AUTH_CLASS

* Tue Apr 29 2003 Ivan F. Martinez <ivanfm@users.sourceforge.net>
- Added support for stunnel 4.x (available on RH 9)

* Wed Feb 26 2003 Ivan F. Martinez <ivanfm@users.sourceforge.net>
- Added xinetd configuration for stunnel support

* Sun Feb 23 2003 Ivan F. Martinez <ivanfm@users.sourceforge.net>
- Changed BuildRoot to use %{_tmppath}

* Thu Jan 02 2003 Ivan F. Martinez <ivanfm@users.sourceforge.net>
- Support for --with and --without build options
- Added more Java path's
- Added more pgsql and mysql path's

* Sun Jun 17 2001 Ivan F. Martinez <ivanfm@users.sourceforge.net>
- Support for init.d.
- Renamed the xinetd service from teapop to teapop_xinetd

* Sat Jun 16 2001 Ivan F. Martinez <ivanfm@users.sourceforge.net>
- Included the config file for xinetd.d
- Included checking for IBMJava
- configure now enable-flock to work with RedHat mailspool 

* Wed Jun 13 2001 Ivan F. Martinez <ivanfm@users.sourceforge.net>
- package created
