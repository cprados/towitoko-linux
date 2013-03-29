# #   doc/Makefile.am
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

EXTRA_DIST = reader.conf design.sgml design.html \
	ctapi.sgml ctapi.3 ctbcs.sgml ctbcs.3 Info.plist
man_MANS = ctapi.3 ctbcs.3
noinst_DATA = design.html

%.3 : %.sgml
if HAVE_DOCBOOK_TO_MAN
	docbook-to-man $<  >$@
else
	: Warning: missing docbook-to-man, cannot make $@
	echo ".TH $< 1"   >$@
	echo "No man page due to missing docbook-to-man" >>$@
endif

%.html : %.sgml
	-sgmltools -b onehtml $<

install-data-hook:
	(cd $(DESTDIR)$(man3dir) && \
	test -f CT_init.3  || $(LN_S) $(shell echo ctapi | sed '$(transform)').3 CT_init.3 && \
	test -f CT_data.3  || $(LN_S) $(shell echo ctapi | sed '$(transform)').3 CT_data.3 && \
	test -f CT_close.3 || $(LN_S) $(shell echo ctapi | sed '$(transform)').3 CT_close.3)
if INSTALL_USB_BUNDLE
	install -D -m 644 Info.plist "@PCSC_LITE_DIR@/drivers/towitoko.bundle/Contents/Info.plist"
endif

uninstall-hook:
	(cd $(DESTDIR)$(man3dir) && rm -f CT_init.3 CT_data.3 CT_close.3)
if INSTALL_USB_BUNDLE
	rm -f "@PCSC_LITE_DIR@/drivers/towitoko.bundle/Contents/Info.plist"
endif

