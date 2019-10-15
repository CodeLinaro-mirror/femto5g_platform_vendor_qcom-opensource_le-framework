##### audioutils #####
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = audio_utils.pc
EXTRA_DIST = $(pkgconfig_DATA)

audioutils_includedir = $(includedir)/audio_utils
audioutils_include_HEADERS  = include/primitives.h
audioutils_include_HEADERS += include/format.h
audioutils_include_HEADERS += include/channels.h

audioutils_sources = format.c \
            channels.c \
            primitives.c

lib_LTLIBRARIES = libaudioutils.la
libaudioutils_la_SOURCES = $(audioutils_sources)
libaudioutils_la_CFLAGS  = -I$(top_srcdir)/include
libaudioutils_la_CFLAGS += -Werror -Wall
libaudioutils_la_LDFLAGS = -shared

##### tinysndfile #####
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA += tinysndfile.pc
EXTRA_DIST = $(pkgconfig_DATA)

tinysnd_includedir = $(includedir)/tinysndfile
tinysnd_include_HEADERS = include/sndfile.h

tinysnd_sources = tinysndfile.c \
            primitives.c

lib_LTLIBRARIES += libtinysndfile.la
libtinysndfile_la_SOURCES = $(tinysnd_sources)
libtinysndfile_la_CFLAGS  = -I$(top_srcdir)/include
libtinysndfile_la_CFLAGS += -Werror -Wall
