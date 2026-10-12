Name:           screensaver-service
Version:        0.2.0
Release:        1%{?dist}
Summary:        Screen savers for Wayland desktops

License:        MIT
URL:            https://github.com/mottipython/screensaver-service
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  meson
BuildRequires:  scdoc
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib
BuildRequires:  systemd-rpm-macros
BuildRequires:  qt6-rpm-macros
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-protocols) >= 1.27
BuildRequires:  pkgconfig(wayland-scanner)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xrandr)
BuildRequires:  cmake(Qt6Widgets)
BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  cmake(KF6KCMUtils)
BuildRequires:  cmake(KF6CoreAddons)

# X11 screen savers are shown through Xwayland; standalone ones do not need it
Recommends:     xorg-x11-server-Xwayland
# the screen savers themselves
Suggests:       xscreensaver-extras-gss
Suggests:       xscreensaver-gl-extras-gss

%description
A small service that starts a screen saver when a Wayland session has been
idle, and a classic settings window to choose one.

Idleness is reported by the compositor (ext-idle-notify-v1), so programs that
inhibit idling, such as video players, hold the screen saver off. The service
runs X11 screen savers through Xwayland and standalone full-screen ones
directly. It does not lock the screen; that is left to the desktop.

%package        kcm
Summary:        Screen saver page for KDE System Settings
Requires:       %{name}%{?_isa} = %{version}-%{release}
Supplements:    (%{name} and systemsettings)

%description    kcm
A page in KDE System Settings, under Display and Monitor, for choosing a screen
saver for screensaver-service.

%prep
%autosetup

%build
%meson
%meson_build

%install
%meson_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/screensaver-settings.desktop
appstream-util validate-relax --nonet %{buildroot}%{_metainfodir}/*.metainfo.xml

%post
%systemd_user_post screensaverd.service

%preun
%systemd_user_preun screensaverd.service

%postun
%systemd_user_postun_with_restart screensaverd.service

%files
%license LICENSE
%doc README.md
%{_bindir}/screensaverd
%{_bindir}/screensaverctl
%{_bindir}/screensaver-settings
%{_userunitdir}/screensaverd.service
%{_datadir}/applications/screensaver-settings.desktop
%{_metainfodir}/io.github.mottipython.ScreensaverService.metainfo.xml
%{_mandir}/man1/screensaverd.1*
%{_mandir}/man1/screensaverctl.1*
%{_mandir}/man1/screensaver-settings.1*

%files kcm
%{_qt6_plugindir}/plasma/kcms/systemsettings_qwidgets/kcm_screensaverservice.so

%changelog
* Mon Oct 12 2026 Motti Schleider <motti.schleider@gmail.com> - 0.2.0-1
- Initial package
