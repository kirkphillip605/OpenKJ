# Disables stripping of debug info
%global _enable_debug_package 0
%global debug_package %{nil}
%global __os_install_post /usr/lib/rpm/brp-compress %{nil}
# End debug strip disable

Name:           openkj
Version:		2.0.5
Release:        5%{?dist}
Summary:        Karaoke show hosting software

License:        GPL
URL:            https://openkj.org
Source0:	https://github.com/OpenKJ/OpenKJ/releases/download/v2.0.5-release/openkj-2.0.5-release.tar.gz

BuildRequires:  cmake qt5-qtbase-devel qt5-qtsvg-devel qt5-qtmultimedia-devel gstreamer1-devel gstreamer1-plugins-base-devel taglib-devel taglib-extras-devel
Requires:       qt5-qtbase qt5-qtsvg qt5-qtmultimedia gstreamer1 gstreamer1-plugins-good gstreamer1-plugins-bad-free gstreamer1-plugins-ugly-free unzip gstreamer1-libav taglib taglib-extras google-roboto-fonts google-roboto-mono-fonts

%description
Karaoke hosting software targeted at professional KJ's.  Includes rotation management, break music player,
key changer, and all of the various bits and pieces required to host karaoke.

%undefine _hardened_build

%prep
%setup

%build
%cmake -DCMAKE_BUILD_TYPE=Debug
%cmake_build

%install
%cmake_install

%files
/usr/bin/openkj
/usr/share/applications/openkj.desktop
/usr/share/pixmaps/okjicon.svg

%changelog
* Tue Aug 15 2017 T. Isaac Lightburn <isaac@hozed.net>
- 
