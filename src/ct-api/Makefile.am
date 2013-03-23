#
#   ct-api/Makefile.am 
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

INC = cardterminal.h ct_slot.h ct_list.h ctapi.h ctbcs.h 
SRC = cardterminal.c ct_slot.c ct_list.c ctapi.c

noinst_LTLIBRARIES = libtowitoko-ctapi.la 
INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/driver
include_HEADERS = ctapi.h ctbcs.h

libtowitoko_ctapi_la_SOURCES = $(SRC) $(INC)
