%define version   0.5
%define release   b2

Name:		maynard
Summary:	A desktop shell client for Weston
Version:	%{version}
Release:	%{release}
Group:		User Interface/Desktops
License:	GPL
URL:		https://github.com/raspberrypi/maynard/
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires:	intltool, libtool, alsa-lib-devel, gnome-menus-devel, gsettings-desktop-schemas-devel
BuildRequires:	gnome-desktop3-devel, libwayland-server-devel, weston-devel

#BuildArchitectures: i686


%description
A desktop shell client for Weston based on GTK.


%prep
%setup -q

%build
./autogen.sh
./configure --prefix=/usr --libdir=%{_libdir}
make CC=clang %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT

make install DESTDIR=%{buildroot}

mkdir -p %{buildroot}/etc/skel/.config/
cp weston.ini %{buildroot}/etc/skel/.config/

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
%{_bindir}/maynard
%{_libdir}/weston/shell-helper.*
%{_libexecdir}/maynard
%{_datadir}/glib-2.0/schemas
/etc/skel/.config/weston.ini


%changelog
* Mon Dec 1 2014 Yuichiro Nakada <berry@berry-lab.net>
- Create for Berry Linux

