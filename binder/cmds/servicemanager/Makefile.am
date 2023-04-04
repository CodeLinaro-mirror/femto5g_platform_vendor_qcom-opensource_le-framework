AM_CPPFLAGS = -I.

bin_PROGRAMS = servicemanager

servicemanager_SOURCES  := service_manager.c binder.c
servicemanager_CPPFLAGS := -I$(top_srcdir)/include
if IPC_32BIT
servicemanager_CPPFLAGS += -DBINDER_IPC_32BIT=1
endif
servicemanager_LDFLAGS = -llog -lcutils

if SELINUX_ENABLE
servicemanager_LDFLAGS  += -lselinux
servicemanager_CFLAGS := -DSELINUX_IS_ENABLE
endif
