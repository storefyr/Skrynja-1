Name:       harbour-skrynja-1
Summary:    ACP-klient til Sailfish OS
Version:    0.1.0
Release:    1
Group:      Applications/Internet
License:    GPL-3.0-only
URL:        https://github.com/storefyr/Skrynja-1
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  desktop-file-utils

%description
Skrynja-1 er en ACP-klient til Sailfish OS. Klienten starter en lokal agent
(vibe-acp) som underordnet proces og taler ACP med den over standard-ind
og -ud.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5 "VERSION=%{version}"

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

desktop-file-install --delete-original \
  --dir %{buildroot}%{_datadir}/applications \
  %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
