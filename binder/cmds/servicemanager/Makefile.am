root_libdir = "@rootprefix@/lib"
root_etcdir = "/etc"

AM_CPPFLAGS = -I.

bin_PROGRAMS = servicemanager

servicemanager_SOURCES  = service_manager.c binder.c
servicemanager_CPPFLAGS = -I$(top_srcdir)/include
if IPC_32BIT
servicemanager_CPPFLAGS += -DBINDER_IPC_32BIT=1
endif
servicemanager_LDFLAGS = -llog -lcutils

if SELINUX_ENABLE
servicemanager_LDFLAGS += -lselinux
servicemanager_CFLAGS = -DSELINUX_IS_ENABLE
endif

if USE_SYSTEMD
binder_systemddir    = $(root_libdir)/systemd/system
binder_systemd_DATA  = $(srcdir)/services/servicemanager.service
binder_systemd_DATA += $(srcdir)/services/binderfs.service
binder_udevdir       = $(root_etcdir)/udev/rules.d
binder_udev_DATA     = $(srcdir)/50-binder.rules
endif
