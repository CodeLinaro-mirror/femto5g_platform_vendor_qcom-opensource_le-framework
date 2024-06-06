Name            : camera_metadata
Version         : 1.0
Release         : r0
Summary         : Recipe to provide Camera Metadata library
License         : Apache-2.0 & BSD-3-Clause-Clear
URL             : https://www.codelinaro.org/
Source0         : %{name}-%{version}.tar.gz

BuildRequires   : autoconf automake libtool gcc-g++ systemd-rpm-macros libcutils libcutils-dev


%description
This package provides Camera Metadata library one of
 the basic functionalities for Linux framework .


%package -n %{name}-dev
Summary: utils library for Development files

%description -n %{name}-dev
This package contains header files, and related items necessary
for software development.


%prep
%autosetup -n camera_metadata

%build
autoreconf -if
%configure --with-core-includes=%{_builddir}/include

%make_build

%install
%make_install

%files
%{_libdir}/libcamera_metadata.a
%{_libdir}/libcamera_metadata.la
%{_libdir}/libcamera_metadata.so.0
%{_libdir}/libcamera_metadata.so.0.0.0

%files -n %{name}-dev
%{_libdir}/libcamera_metadata.so
%{_includedir}/camera/camera_metadata_hidden.h
%{_includedir}/system/camera_metadata.h
%{_includedir}/system/camera_metadata_tags.h
%{_includedir}/system/camera_vendor_tags.h