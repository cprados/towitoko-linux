#
#   driver/Makefile.am 
#
#   This file is part of the Unix driver for Towitoko smartcard readers  
#   Copyright (C) 2000 Carlos Prados <cprados@yahoo.com>
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public
#   License as published by the Free Software Foundation; either
#   version 2 of the License, or (at your option) any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Lesser General Public License for more details.
#
#   You should have received a copy of the GNU Lesser General Public
#   License along with this library; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

VER = 2:0:0
INC = atr.h icc_async.h io_serial.h protocol_t0.h protocol_t1.h t1_block.h \
	ifd_towitoko.h defines.h icc_sync.h apdu.h protocol_sync.h atr_sync.h \
	pps.h tlv_object.h
SRC = atr.c icc_async.c io_serial.c protocol_t0.c protocol_t1.c t1_block.c \
	ifd_towitoko.c apdu.c icc_sync.c protocol_sync.c atr_sync.c pps.c \
	tlv_object.c

lib_LTLIBRARIES = libtowitoko.la
INCLUDES = -I$(top_srcdir)

libtowitoko_la_SOURCES = $(SRC) $(INC)
libtowitoko_la_LDFLAGS = -version-info $(VER) -module 
libtowitoko_la_LIBADD = $(top_builddir)/src/ct-api/libtowitoko-ctapi.la \
		$(top_builddir)/src/ifd-handler/libtowitoko-ifdhandler.la

install-exec-hook:
if INSTALL_USB_BUNDLE
	install -d "@PCSC_LITE_DIR@/drivers/towitoko.bundle/Contents/Linux" 
	(cd "@PCSC_LITE_DIR@/drivers/towitoko.bundle/Contents/Linux" && \
	test -f libtowitoko.so.2.0.0 || $(LN_S) @libdir@/libtowitoko.so.2.0.0 .)
endif

uninstall-hook:
if INSTALL_USB_BUNDLE
	rm -rf "@PCSC_LITE_DIR@/drivers/towitoko.bundle"
endif

