AM_CPPFLAGS = -I.

bin_PROGRAMS = servicemanager

servicemanager_SOURCES  := service_manager.c binder.c
servicemanager_CPPFLAGS := -I$(top_srcdir)/include
if IPC_32BIT
servicemanager_CPPFLAGS += -DBINDER_IPC_32BIT=1
endif
servicemanager_LDFLAGS = -llog -lcutils

if VENDOR_BINDER_ENABLE
bin_PROGRAMS += vndservicemanager

vndservicemanager_SOURCES  := service_manager.c binder.c
vndservicemanager_CPPFLAGS := -I$(top_srcdir)/include
vndservicemanager_CPPFLAGS += -D__ANDROID_VNDK__
if IPC_32BIT
vndservicemanager_CPPFLAGS += -DBINDER_IPC_32BIT=1
endif
vndservicemanager_LDFLAGS = -llog -lcutils
endif

if SELINUX_ENABLE
servicemanager_LDFLAGS  += -lselinux
servicemanager_CFLAGS := -DSELINUX_IS_ENABLE
if VENDOR_BINDER_ENABLE
vndservicemanager_LDFLAGS  += -lselinux
vndservicemanager_CFLAGS := -DSELINUX_IS_ENABLE
endif
endif
