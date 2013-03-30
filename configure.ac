dnl configure.ac
dnl Process this file with autoconf to produce a configure script.
dnl
dnl This file is part of the Unix driver for Towitoko smartcard readers
dnl Copyright (c) 2000 Carlos Prados <cprados@yahoo.com>

AC_INIT

AM_INIT_AUTOMAKE(towitoko,2.0.8)

AM_CONFIG_HEADER(config.h)

AC_PREFIX_DEFAULT(/usr/local/towitoko)

#----------------------------------------------------------------------------
# 	Options for setting fix card type / memory size
#----------------------------------------------------------------------------

AC_ARG_ENABLE(card-type,
AC_HELP_STRING([--enable-card-type(=sync|async)],
[card type (default=probe)]))

case "$enable_card_type" in
  sync)
  AC_DEFINE(ICC_TYPE_SYNC,1,[Smartcard of type synchronoys])
  ;;
  async)
  AC_DEFINE(ICC_TYPE_ASYNC,1,[Smartcard of type asynchronoys])
  ;;
esac

AC_ARG_ENABLE(card-convention,
AC_HELP_STRING([--enable-card-convention(=dir|inv)],
[card convention (default=probe)]))

case "$enable_card_convention" in
  dir)
  AC_DEFINE(IFD_TOWITOKO_CONVENTION_DIRECT,1,
  [Smartcard with direct convention])
  ;;
  inv)
  AC_DEFINE(IFD_TOWITOKO_CONVENTION_INVERSE,1,
  [Smartcard with inverse convention])
  ;;
esac

AC_ARG_ENABLE(memory-type,
AC_HELP_STRING([--enable-memory-type(=i2cs|i2cl|2w|3w)],
[memory card type (default=probe)]))

case "$enable_memory_type" in
  i2cs)
    memory_type=0;
  ;;
  i2cl)
    memory_type=1;
  ;;
  2w)
    memory_type=2;
  ;;
  3w)
    memory_type=3;
  ;;
esac

if test "$memory_type" != "" ; then
  AC_DEFINE_UNQUOTED(ICC_SYNC_MEMORY_TYPE,$memory_type,
  [Memory smartcard type (0,1,2,3)])
fi

AC_ARG_ENABLE(memory-size,
AC_HELP_STRING([--enable-memory-size(=size)],[memory card size (default=probe)]))

if test "$enable_memory_size" != ""; then
  AC_DEFINE_UNQUOTED(ICC_SYNC_MEMORY_LENGTH, $enable_memory_size, [Memory size])
fi

AC_ARG_ENABLE(ISO-T0,
AC_HELP_STRING([--enable-ISO-t0], [transportation of APDUs by T=0 (default=no)]))

if test "$enable_ISO_t0" = "true"; then
  AC_DEFINE_UNQUOTED(PROTOCOL_T0_ISO, 1, [Transportation of APDUs by T=0])
fi
#----------------------------------------------------------------------------
# 	Debug level
#----------------------------------------------------------------------------

AC_ARG_ENABLE(debug,
AC_HELP_STRING([--enable-debug(=1|2|3|4)],[ enable debug (default=no)]))

if test "$enable_debug" = "yes"; then
  enable_debug=3
else
  if test "$enable_debug" = "no"; then
    enable_debug=0
  fi
fi

if test "$enable_debug" != ""; then
  if test "$enable_debug" -lt "4"; then
    if test "$enable_debug" -gt "0"; then
      AC_DEFINE(DEBUG_CTAPI,1,[Debug CT-API])
      AC_DEFINE(DEBUG_PROTOCOL,1,[Debug Protocol])
      AC_DEFINE(DEBUG_ICC,1,[Debug Integrated Circuit Card])
    fi

    if test "$enable_debug" -gt "1"; then
      AC_DEFINE(DEBUG_IFD,1,[Debug Interface Device])
    fi

    if test "$enable_debug" -gt "2"; then
      AC_DEFINE(DEBUG_IO,1,[Debug Input/Output])
    fi
  else
    AC_DEFINE(DEBUG_IFDH,1,[Debug IFD Handler])
  fi
fi

#----------------------------------------------------------------------------
# 	Option to set PCSC Lite base directory
#----------------------------------------------------------------------------

AC_ARG_WITH(pcsc-lite-dir,
AC_HELP_STRING([--with-pcsc-lite-dir],
[PCSC Lite base directory (default=/usr/local/pcsc)]),
PCSC_LITE_DIR="$with_pcsc_lite_dir", PCSC_LITE_DIR="/usr/local/pcsc")

AC_SUBST(PCSC_LITE_DIR)

#----------------------------------------------------------------------------
# 	Option to enable USB bundle installation
#----------------------------------------------------------------------------

AC_ARG_ENABLE(usb-bundle,
AC_HELP_STRING([--enable-usb-bundle],[enable PCSC Lite USB bundle installation (default=no)]))

AM_CONDITIONAL(INSTALL_USB_BUNDLE, test "$enable_usb_bundle" = "yes")

#----------------------------------------------------------------------------
# 	Option to enable devfs support
#----------------------------------------------------------------------------

AC_ARG_ENABLE(devfs,
AC_HELP_STRING([--enable-devfs],
[enable Linux 2.4.X devfs support (default=no)]))

if test "$enable_devfs" = "yes"; then
  AC_DEFINE(IO_ENABLE_DEVFS, 1, [Enable Linux 2.4.X devfs support])
fi

#----------------------------------------------------------------------------
# 	Option to enable /dev/pcsc/ links support
#----------------------------------------------------------------------------

AC_ARG_ENABLE(devpcsc,
AC_HELP_STRING([--enable-devpcsc],
[enable /dev/pcsc links support (default=no)]))

if test "$enable_devpcsc" = "yes"; then
  AC_DEFINE(IO_ENABLE_DEVPCSC, 1, [Enable /dev/pcsc/ links])
fi

#----------------------------------------------------------------------------
# 	Option to enable win32 style COM port numbering
#----------------------------------------------------------------------------

AC_ARG_ENABLE(win32-com,
AC_HELP_STRING([--enable-win32-com],
[enable win32 style COM port numbering (default=no)]))

if test "$enable_win32_com" = "yes"; then
  AC_DEFINE(CTAPI_WIN32_COM, 1, [Enable win32 COM numbering])
fi

#----------------------------------------------------------------------------
# 	Option to disable multi-threading
#----------------------------------------------------------------------------

AC_ARG_ENABLE(thread-mutex,
AC_HELP_STRING([--enable-thread-mutex],[enable thread mutexes (default=yes)]))

#----------------------------------------------------------------------------
# 	Option for Atr cheking
#----------------------------------------------------------------------------

AC_ARG_ENABLE(atr-check,
AC_HELP_STRING([--enable-atr-check],
[enable checking of valid ATR (default=yes)]))

if test "$enable_atr_check" = "" || test "$enable_atr_check" = "yes"; then
  AC_DEFINE(IFD_TOWITOKO_STRICT_ATR_CHECK,1,
  [ATR for asynchronous cards is cheked])
fi

#----------------------------------------------------------------------------
# 	Option for Atr timings cheking
#----------------------------------------------------------------------------

AC_ARG_ENABLE(atr-timings,
AC_HELP_STRING([--enable-atr-timings],
[enable decoding of timings from ATR (default=yes)]))

if test "$enable_atr_timings" = "no"; then
  AC_DEFINE(PROTOCOL_T0_USE_DEFAULT_TIMINGS,1,
  [Timings in ATR are not used in T=0 cards])
  AC_DEFINE(PROTOCOL_T1_USE_DEFAULT_TIMINGS,1,
  [Timings in ATR are not used in T=1 cards])
  AC_DEFINE(PTS_USE_DEFAULT_TIMINGS,1,
  [Timings in ATR are not used after PTS])
fi

#----------------------------------------------------------------------------
#	Check environment
#---------------------------------------------------------------------------

AC_CANONICAL_HOST
case $host in
  *linux*)
  AC_DEFINE(OS_LINUX,1,[Linux Operating System])
  AC_DEFINE(IO_ENABLE_USB, 1, [Enable USB support])
  AC_DEFINE(_REENTRANT,1,[This may be usefull for LinuxThreads])
  ;;
  *cygwin*)
  AC_DEFINE(OS_CYGWIN32,1,[Cywin32 for Windows NT environment])
  AC_DEFINE(CTAPI_WIN32_COM, 1, [Enable win32 COM numbering])
  ;;
  *solaris*)
  AC_DEFINE(OS_SOLARIS,1,[Sun Operating System])
  ;;
  *hp-hpux*)
  AC_DEFINE(OS_HPUX,1,[HPUX Operating System])
  ;;
  *sgi-irix*)
  AC_DEFINE(OS_IRIX,1,[IRIX Operating System])
  ;;
  *sco*)
  AC_DEFINE(OS_SCO,1,[SCO Operating System])
  ;;
  *openbsd*)
  AC_DEFINE(OS_OPENBSD,1,[OpenBSD Operating System])
  ;;
  *freebsd*)
  AC_DEFINE(OS_FREEBSD,1,[FreeBSD Operating System])
  ;;
  *aix*)
  AC_DEFINE(OS_AIX,1,[FreeBSD Operating System])
  ;;
  *netbsd*)
  AC_DEFINE(OS_NETBSD,1,[FreeBSD Operating System])
  ;;
  *)
  AC_DEFINE(OS_OTHER, 1, [Other Operating System])
  AC_DEFINE(IO_ENABLE_DEVPCSC, 1, [Enable /dev/pcsc/ links])
  ;;
esac

AC_PROG_CC

AM_DISABLE_STATIC
AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL

AC_CHECK_PROG(DOCBOOK_TO_MAN, docbook-to-man, yes, no)
AM_CONDITIONAL(HAVE_DOCBOOK_TO_MAN, test "$ac_cv_prog_DOCBOOK_TO_MAN" = yes)

AC_PROG_LN_S

if test "$enable_thread_mutex" = "yes" || test "$enable_thread_mutex" = "";
then
  AC_CHECK_LIB(pthread, main, [AC_CHECK_HEADERS(pthread.h)
  LIBS="$LIBS -lpthread"])
fi

AC_CHECK_HEADERS(sys/time.h)

dnl Check for libraries
AC_SEARCH_LIBS(nanosleep, rt posix posix4)

dnl Checks for functions.
AC_CHECK_FUNCS(poll)
AC_CHECK_FUNCS(nanosleep)
AC_CHECK_FUNCS(syslog) 


#----------------------------------------------------------------------------
#	Generate Makefiles
#----------------------------------------------------------------------------

AC_OUTPUT(Makefile src/Makefile src/driver/Makefile src/ct-api/Makefile
	src/ifd-handler/Makefile src/test/Makefile doc/Makefile)
