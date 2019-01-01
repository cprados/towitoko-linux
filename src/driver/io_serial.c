/*
    io_serial.c
    Serial port input/output functions

    This file is part of the Unix driver for Towitoko smartcard readers
    Copyright (C) 2000 2001 Carlos Prados <cprados@yahoo.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "defines.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_POLL
#include <sys/poll.h>
#else
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/time.h>
#endif
#include <sys/ioctl.h>
#include <time.h>
#include "io_serial.h"

#define IO_SERIAL_FILENAME_LENGTH 	32

/*
 * Internal functions declaration
 */

static int
IO_Serial_Bitrate(int bitrate);

static bool
IO_Serial_WaitToRead (int hnd, unsigned delay_ms, unsigned timeout_ms);

static bool 
IO_Serial_WaitToWrite (int hnd, unsigned delay_ms, unsigned timeout_ms);

static void 
IO_Serial_DeviceName (unsigned com, bool usbserial, char * filename, unsigned length);

static bool 
IO_Serial_InitPnP (IO_Serial * io);

static void 
IO_Serial_Clear (IO_Serial * io);

static bool 
IO_Serial_GetPropertiesCache(IO_Serial * io, IO_Serial_Properties * props);

static void
IO_Serial_SetPropertiesCache(IO_Serial * io, IO_Serial_Properties * props);

static void
IO_Serial_ClearPropertiesCache (IO_Serial * io);

/*
 * Public functions definition
 */

IO_Serial *
IO_Serial_New (void)
{
  IO_Serial *io;

  io = (IO_Serial *) malloc (sizeof (IO_Serial));

  if (io != NULL)
    IO_Serial_Clear (io);

  return io;
}

bool IO_Serial_Init (IO_Serial * io, unsigned com, bool usbserial, bool pnp)
{
  char filename[IO_SERIAL_FILENAME_LENGTH];

  IO_Serial_DeviceName (com, usbserial, filename, IO_SERIAL_FILENAME_LENGTH);

#ifdef DEBUG_IO
  printf ("IO: Opening serial port %s\n", filename);
#endif

  if (com < 1)
    return FALSE;

  io->com = com;
  io->fd = open (filename, O_RDWR | O_NOCTTY);

  if (io->fd < 0)
    return FALSE;

  if (pnp)
    IO_Serial_InitPnP (io);

  io->usbserial=usbserial;

  return TRUE;
}

bool
IO_Serial_GetProperties (IO_Serial * io, IO_Serial_Properties * props)
{
  struct termios currtio;
  speed_t i_speed, o_speed;
  unsigned int mctl;

  if (IO_Serial_GetPropertiesCache(io, props))
    return TRUE;

  if (tcgetattr (io->fd, &currtio) != 0)
    return FALSE;

  o_speed = cfgetospeed (&currtio);

  switch (o_speed)
    {
#ifdef B0
    case B0:
      props->output_bitrate = 0;
      break;
#endif
#ifdef B50
    case B50:
      props->output_bitrate = 50;
      break;
#endif
#ifdef B75
    case B75:
      props->output_bitrate = 75;
      break;
#endif
#ifdef B110
    case B110:
      props->output_bitrate = 110;
      break;
#endif
#ifdef B134
    case B134:
      props->output_bitrate = 134;
      break;
#endif
#ifdef B150
    case B150:
      props->output_bitrate = 150;
      break;
#endif
#ifdef B200
    case B200:
      props->output_bitrate = 200;
      break;
#endif
#ifdef B300
    case B300:
      props->output_bitrate = 300;
      break;
#endif
#ifdef B600
    case B600:
      props->output_bitrate = 600;
      break;
#endif
#ifdef B1200
    case B1200:
      props->output_bitrate = 1200;
      break;
#endif
#ifdef B1800
    case B1800:
      props->output_bitrate = 1800;
      break;
#endif
#ifdef B2400
    case B2400:
      props->output_bitrate = 2400;
      break;
#endif
#ifdef B4800
    case B4800:
      props->output_bitrate = 4800;
      break;
#endif
#ifdef B9600
    case B9600:
      props->output_bitrate = 9600;
      break;
#endif
#ifdef B19200
    case B19200:
      props->output_bitrate = 19200;
      break;
#endif
#ifdef B38400
    case B38400:
      props->output_bitrate = 38400;
      break;
#endif
#ifdef B57600
    case B57600:
      props->output_bitrate = 57600;
      break;
#endif
#ifdef B115200
    case B115200:
      props->output_bitrate = 115200;
      break;
#endif
#ifdef B230400
    case B230400:
      props->output_bitrate = 230400;
      break;
#endif
    default:
      props->output_bitrate = 1200;
      break;
    }

  i_speed = cfgetispeed (&currtio);

  switch (i_speed)
    {
#ifdef B0
    case B0:
      props->input_bitrate = 0;
      break;
#endif
#ifdef B50
    case B50:
      props->input_bitrate = 50;
      break;
#endif
#ifdef B75
    case B75:
      props->input_bitrate = 75;
      break;
#endif
#ifdef B110
    case B110:
      props->input_bitrate = 110;
      break;
#endif
#ifdef B134
    case B134:
      props->input_bitrate = 134;
      break;
#endif
#ifdef B150
    case B150:
      props->input_bitrate = 150;
      break;
#endif
#ifdef B200
    case B200:
      props->input_bitrate = 200;
      break;
#endif
#ifdef B300
    case B300:
      props->input_bitrate = 300;
      break;
#endif
#ifdef B600
    case B600:
      props->input_bitrate = 600;
      break;
#endif
#ifdef B1200
    case B1200:
      props->input_bitrate = 1200;
      break;
#endif
#ifdef B1800
    case B1800:
      props->input_bitrate = 1800;
      break;
#endif
#ifdef B2400
    case B2400:
      props->input_bitrate = 2400;
      break;
#endif
#ifdef B4800
    case B4800:
      props->input_bitrate = 4800;
      break;
#endif
#ifdef B9600
    case B9600:
      props->input_bitrate = 9600;
      break;
#endif
#ifdef B19200
    case B19200:
      props->input_bitrate = 19200;
      break;
#endif
#ifdef B38400
    case B38400:
      props->input_bitrate = 38400;
      break;
#endif
#ifdef B57600
    case B57600:
      props->input_bitrate = 57600;
      break;
#endif
#ifdef B115200
    case B115200:
      props->input_bitrate = 115200;
      break;
#endif
#ifdef B230400
    case B230400:
      props->input_bitrate = 230400;
      break;
#endif
    default:
      props->input_bitrate = 1200;
      break;
    }

  switch (currtio.c_cflag & CSIZE)
    {
    case CS5:
      props->bits = 5;
      break;
    case CS6:
      props->bits = 6;
      break;
    case CS7:
      props->bits = 7;
      break;
    case CS8:
      props->bits = 8;
      break;
    }

  if (((currtio.c_cflag) & PARENB) == PARENB)
    if (((currtio.c_cflag) & PARODD) == PARODD)
      props->parity = IO_SERIAL_PARITY_ODD;
    else
      props->parity = IO_SERIAL_PARITY_EVEN;
  else
    props->parity = IO_SERIAL_PARITY_NONE;

  if (((currtio.c_cflag) & CSTOPB) == CSTOPB)
    props->stopbits = 2;
  else
    props->stopbits = 1;

#if !defined(OS_CYGWIN32) && !defined(OS_HPUX)
  if (ioctl (io->fd, TIOCMGET, &mctl) < 0)
    return FALSE;

  props->dtr = ((mctl & TIOCM_DTR) ? IO_SERIAL_HIGH : IO_SERIAL_LOW);
  props->rts = ((mctl & TIOCM_RTS) ? IO_SERIAL_HIGH : IO_SERIAL_LOW);
#else
  props->dtr = IO_SERIAL_HIGH;
  props->rts = IO_SERIAL_HIGH;
#endif

  IO_Serial_SetPropertiesCache (io, props);

#ifdef DEBUG_IO
  printf
    ("IO: Getting properties: %ld bps; %d bits/byte; %s parity; %d stopbits; dtr=%d; rts=%d\n",
     props->input_bitrate, props->bits,
     props->parity == IO_SERIAL_PARITY_EVEN ? "Even" : props->parity ==
     IO_SERIAL_PARITY_ODD ? "Odd" : "None", props->stopbits, props->dtr,
     props->rts);
#endif

  return TRUE;
}

bool
IO_Serial_SetProperties (IO_Serial * io, IO_Serial_Properties * props)
{
  struct termios newtio;
  unsigned int modembits;
#if 1
#if !defined(OS_CYGWIN32) && !defined(OS_HPUX)

  modembits = TIOCM_DTR;

  if (props->dtr == IO_SERIAL_HIGH)
    {
      if (ioctl (io->fd, TIOCMBIS, &modembits) < 0)
	return FALSE;
    }

  else if (props->dtr == IO_SERIAL_LOW)
    {
      if (ioctl (io->fd, TIOCMBIC, &modembits) < 0)
	return FALSE;
    }

  modembits = TIOCM_RTS;

  if (props->rts == IO_SERIAL_HIGH)
    {
      if (ioctl (io->fd, TIOCMBIS, &modembits) < 0)
	return FALSE;
    }

  else if (props->rts == IO_SERIAL_LOW)
    {
      if (ioctl (io->fd, TIOCMBIC, &modembits) < 0)
	return FALSE;
    }

#endif
#endif

  memset (&newtio, 0, sizeof (newtio));

  /* Set the bitrate */
  cfsetispeed (&newtio, IO_Serial_Bitrate (props->input_bitrate));
  cfsetospeed (&newtio, IO_Serial_Bitrate (props->output_bitrate));

  /* Set the character size */
  switch (props->bits)
    {
    case 5:
      newtio.c_cflag |= CS5;
      break;

    case 6:
      newtio.c_cflag |= CS6;
      break;

    case 7:
      newtio.c_cflag |= CS7;
      break;

    case 8:
      newtio.c_cflag |= CS8;
      break;
    }

  /* Set the parity */
  switch (props->parity)
    {
    case IO_SERIAL_PARITY_ODD:
      newtio.c_cflag |= PARENB;
      newtio.c_cflag |= PARODD;
      break;

    case IO_SERIAL_PARITY_EVEN:

      newtio.c_cflag |= PARENB;
      newtio.c_cflag &= ~PARODD;
      break;

    case IO_SERIAL_PARITY_NONE:
      newtio.c_cflag &= ~PARENB;
    }

  /* Set the number of stop bits */
  switch (props->stopbits)
    {
    case 1:
      newtio.c_cflag &= (~CSTOPB);
      break;
    case 2:
      newtio.c_cflag |= CSTOPB;
      break;
    }

  /* Selects raw (non-canonical) input and output */
  newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  newtio.c_oflag &= ~OPOST;
#if 1
  /* Ignore parity errors!!! Windows driver does so why shouldn't I? */
  newtio.c_iflag |= IGNPAR;
#endif
  /* Enable receiber, hang on close, ignore control line */
  newtio.c_cflag |= CREAD | HUPCL | CLOCAL;

  /* Read 1 byte minimun, no timeout specified */
  newtio.c_cc[VMIN] = 1;
  newtio.c_cc[VTIME] = 0;

  if (tcsetattr (io->fd, TCSANOW, &newtio) < 0)
    return FALSE;

  if (tcflush (io->fd, TCIFLUSH) < 0)
    return FALSE;

  IO_Serial_SetPropertiesCache (io, props);

#ifdef DEBUG_IO
  printf
    ("IO: Setting properties: %ld bps; %d bits/byte; %s parity; %d stopbits; dtr=%d; rts=%d\n",
     props->input_bitrate, props->bits,
     props->parity == IO_SERIAL_PARITY_EVEN ? "Even" : props->parity ==
     IO_SERIAL_PARITY_ODD ? "Odd" : "None", props->stopbits, props->dtr,
     props->rts);
#endif
  return TRUE;
}

void
IO_Serial_GetPnPId (IO_Serial * io, BYTE * pnp_id, unsigned *length)
{
  (*length) = io->PnP_id_size;
  memcpy (pnp_id, io->PnP_id, io->PnP_id_size);
}

unsigned 
IO_Serial_GetCom (IO_Serial * io)
{
  return io->com;
}


bool
IO_Serial_Read (IO_Serial * io, unsigned timeout, unsigned size, BYTE * data)
{
  BYTE c;
  int count = 0;

#ifdef DEBUG_IO
  printf ("IO: Receiving: ");
  fflush (stdout);
#endif

  for (count = 0; count < size; count++)
    {
      if (IO_Serial_WaitToRead (io->fd, 0, timeout))
	{
	  if (read (io->fd, &c, 1) != 1)
	    {
#ifdef DEBUG_IO
	      printf ("ERROR\n");
	      fflush (stdout);
#endif
	      return FALSE;
	    }
	  data[count] = c;

#ifdef DEBUG_IO
	  printf ("%X ", c);
	  fflush (stdout);
#endif
	}
      else
	{
#ifdef DEBUG_IO
	  printf ("TIMEOUT\n");
	  fflush (stdout);
#endif
	  /* tcflush (io->fd, TCIFLUSH); */
	  return FALSE;
	}
    }

#ifdef DEBUG_IO
  printf ("\n");
  fflush (stdout);
#endif

  return TRUE;
}

bool
IO_Serial_Write (IO_Serial * io, unsigned delay, unsigned size, BYTE * data)
{
  unsigned count, to_send;
#ifdef DEBUG_IO
  unsigned i;

  printf ("IO: Sending: ");
  fflush (stdout);
#endif
  /* Discard input data from previous commands */
  tcflush (io->fd, TCIFLUSH);

  for (count = 0; count < size; count += to_send)
    {

#ifdef DEBUG_SEND_ONE
      to_send = (delay? 1: size);
#else
      to_send = size;
#endif

      if (IO_Serial_WaitToWrite (io->fd, delay, 1000))
	{
	  if (write (io->fd, data + count, to_send) != to_send)
	    {
#ifdef DEBUG_IO
	      printf ("ERROR\n");
	      fflush (stdout);
#endif
	      return FALSE;
	    }

#ifdef DEBUG_IO
	  for (i=0; i<to_send; i++)
	    printf ("%X ", data[count + i]);
	  fflush (stdout);
#endif
	}
      else
	{
#ifdef DEBUG_IO
	  printf ("TIMEOUT\n");
	  fflush (stdout);
#endif
	  /* tcflush (io->fd, TCIFLUSH); */
	  return FALSE;
	}
    }

#ifdef DEBUG_IO
  printf ("\n");
  fflush (stdout);
#endif

  return TRUE;
}

bool IO_Serial_Close (IO_Serial * io)
{
  char filename[IO_SERIAL_FILENAME_LENGTH];

 IO_Serial_DeviceName (io->com, io->usbserial, filename,
 IO_SERIAL_FILENAME_LENGTH);

#ifdef DEBUG_IO
  printf ("IO: Clossing serial port %s\n", filename);
#endif

  if (close (io->fd) != 0)
    return FALSE;

  IO_Serial_ClearPropertiesCache (io);
  IO_Serial_Clear (io);

  return TRUE;
}

void
IO_Serial_Delete (IO_Serial * io)
{
  if (io->props != NULL)
    free (io->props);

  free (io);
}

/*
 * Internal functions definition
 */

static int
IO_Serial_Bitrate(int bitrate)
{
#ifdef B230400
	if ((bitrate)>=230400) return B230400;
#endif
#ifdef B115200
	if ((bitrate)>=115200) return B115200;
#endif
#ifdef B57600
	if ((bitrate)>=57600) return B57600;
#endif
#ifdef B38400
	if ((bitrate)>=38400) return B38400;
#endif
#ifdef B19200
	if ((bitrate)>=19200) return B19200;
#endif
#ifdef B9600
	if ((bitrate)>=9600) return B9600;
#endif
#ifdef B4800
	if ((bitrate)>=4800) return B4800;
#endif
#ifdef B2400
	if ((bitrate)>=2400) return B2400;
#endif
#ifdef B1800
	if ((bitrate)>=1800) return B1800;
#endif
#ifdef B1200
	if ((bitrate)>=1200) return B1200;
#endif
#ifdef B600
	if ((bitrate)>=600) return B600;
#endif
#ifdef B300
	if ((bitrate)>=300) return B300;
#endif
#ifdef B200
	if ((bitrate)>=200) return B200;
#endif
#ifdef B150
	if ((bitrate)>=150) return B150;
#endif
#ifdef B134
	if ((bitrate)>=134) return B134;
#endif
#ifdef B110
	if ((bitrate)>=110) return B110;
#endif
#ifdef B75
	if ((bitrate)>=75) return B75;
#endif
#ifdef B50
	if ((bitrate)>=50) return B50;
#endif
#ifdef B0
	if ((bitrate)>=0) return B0;
#endif
	return 0;	/* Should never get here */
}

static bool
IO_Serial_WaitToRead (int hnd, unsigned delay_ms, unsigned timeout_ms)
{
  int rval;
#ifdef HAVE_POLL
  struct pollfd ufds;
#else
  fd_set rfds;
  struct timeval tv;
#endif

  if (delay_ms > 0)
    {
#ifdef HAVE_NANOSLEEP
      struct timespec req_ts;

      req_ts.tv_sec = delay_ms / 1000;
      req_ts.tv_nsec = (delay_ms % 1000) * 1000000L;
      nanosleep (&req_ts, NULL);
#else
      usleep ((unsigned long) (delay_ms * 1000L));
#endif
    }

#ifdef HAVE_POLL
  ufds.fd = hnd;
  ufds.events = POLLIN;
  ufds.revents = 0x0000;

  rval = poll (&ufds, 1, timeout_ms);
  if (rval != 1)
    return (FALSE);

  return (((ufds.revents) & POLLIN) == POLLIN);
#else
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000L;

  FD_ZERO (&rfds);
  FD_SET (hnd, &rfds);

  rval = select (hnd + 1, &rfds, NULL, NULL, &tv);

  return FD_ISSET (hnd, &rfds);
#endif
}

static bool
IO_Serial_WaitToWrite (int hnd, unsigned delay_ms, unsigned timeout_ms)
{
  int rval;
#ifdef HAVE_POLL
  struct pollfd ufds;
#else
  fd_set rfds;
  struct timeval tv;
#endif

  if (delay_ms > 0)
    {
#ifdef HAVE_NANOSLEEP
      struct timespec req_ts;

      req_ts.tv_sec = delay_ms / 1000;
      req_ts.tv_nsec = (delay_ms % 1000) * 1000000L;
      nanosleep (&req_ts, NULL);
#else
      usleep ((unsigned long) (delay_ms * 1000L));
#endif
    }

#ifdef HAVE_POLL
  ufds.fd = hnd;
  ufds.events = POLLOUT;
  ufds.revents = 0x0000;

  rval = poll (&ufds, 1, timeout_ms);
  if (rval != 1)
    return (FALSE);

  return (((ufds.revents) & POLLOUT) == POLLOUT);
#else
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000L;

  FD_ZERO (&rfds);
  FD_SET (hnd, &rfds);

  rval = select (hnd + 1, NULL, &rfds, NULL, &tv);

  return FD_ISSET (hnd, &rfds);

#endif
}

static void
IO_Serial_Clear (IO_Serial * io)
{
  io->fd = -1;
  io->props = NULL;
  io->com = 0;
  memset (io->PnP_id, 0, IO_SERIAL_PNPID_SIZE);
  io->PnP_id_size = 0;
  io->usbserial = FALSE;
}

static void
IO_Serial_SetPropertiesCache(IO_Serial * io, IO_Serial_Properties * props)
{
  if (io->props == NULL)
      io->props = (IO_Serial_Properties *) malloc (sizeof (IO_Serial_Properties));
#ifdef DEBUG_IO
  printf ("IO: Catching properties\n");
#endif

  memcpy (io->props, props, sizeof (IO_Serial_Properties)); 
}

static bool
IO_Serial_GetPropertiesCache(IO_Serial * io, IO_Serial_Properties * props)
{
  if (io->props != NULL)
    {
      memcpy (props, io->props, sizeof (IO_Serial_Properties)); 
#if 0
#ifdef DEBUG_IO
    printf
    ("IO: Getting properties (catched): %ld bps; %d bits/byte; %s parity; %d stopbits; dtr=%d; rts=%d\n",
     props->input_bitrate, props->bits, 
     props->parity == IO_SERIAL_PARITY_EVEN ? "Even" : props->parity ==
     IO_SERIAL_PARITY_ODD ? "Odd" : "None", props->stopbits, props->dtr,
     props->rts);
#endif
#endif
      return TRUE;
    }

  return FALSE;
}

static void
IO_Serial_ClearPropertiesCache (IO_Serial * io)
{
#ifdef DEBUG_IO
  printf ("IO: Clearing properties cache\n");
#endif
  if (io->props != NULL)
    {
      free (io->props);
      io->props = NULL;
    }
}

static void
IO_Serial_DeviceName (unsigned com, bool usbserial, char * filename, unsigned length)
{
#ifdef IO_ENABLE_USB
  if (usbserial)

#ifdef OS_LINUX
#ifdef IO_ENABLE_DEVFS
    snprintf (filename, length, "/dev/usb/tts/%d", com - 1);
#else
    snprintf (filename, length, "/dev/ttyUSB%d", com - 1);
#endif
#endif /* OS_LINUX */

  else
#endif /* IO_ENABLE_USB */

#ifdef IO_ENABLE_DEVPCSC
    snprintf (filename, length, "/dev/pcsc/%d", com);
#else

#ifdef OS_LINUX
#ifdef IO_ENABLE_DEVFS
    snprintf (filename, length, "/dev/tts/%d", com - 1);
#else
    snprintf (filename, length, "/dev/ttyS%d", com - 1);
#endif
#endif /* OS_LINUX */

#ifdef OS_SOLARIS
  snprintf (filename, length, "/dev/cua/%c", 'a' + com - 1);
#endif /* OS_SOLARIS */

#ifdef OS_CYGWIN32
  sprintf (filename, "COM%d", com);
#endif /* OS_CYGWIN32 */

#ifdef OS_HPUX
  snprintf (filename, length, "/dev/tty%dp0", com);
#endif /* OS_HPUX */

#ifdef OS_IRIX
  snprintf (filename, length, "/dev/ttyf%d", com);
#endif /* OS_IRIX */

#ifdef OS_DIGITAL_UNIX
  snprintf (filename, length, "/dev/tty%02d", com);
#endif /* OS_DIGITAL_UNIX */

#ifdef OS_AIX
  snprintf (filename, length, "/dev/tty%d", com - 1);
#endif /* OS_AIX */

#ifdef OS_SCO
  snprintf (filename, length, "/dev/tty%da", com);
#endif /* OS_SCO */

#ifdef OS_FREEBSD
  /* support for for FreeBSD added by Martin Preuss<libchipcard@aquamaniac.de> */
  snprintf (filename, length, "/dev/cuaa%d", com - 1);
#endif /* OS_FREEBSD */

#ifdef OS_NETBSD
  snprintf (filename, length, "/dev/ttyC%d", com - 1);
#endif /* OS_NETBSD */

#ifdef OS_OPENBSD
  /* support for for FreeBSD added by Martin Preuss<libchipcard@aquamaniac.de> */
  snprintf (filename, length, "/dev/cua%02d", com - 1);
#endif /* OS_SOLARIS */

#endif /* IO_ENABLE_DEVPCSC */
}

static bool
IO_Serial_InitPnP (IO_Serial * io)
{
  IO_Serial_Properties props;
  int i = 0;

  props.input_bitrate = 1200;
  props.output_bitrate = 1200;
  props.parity = IO_SERIAL_PARITY_NONE;
  props.bits = 7;
  props.stopbits = 1;
  props.dtr = IO_SERIAL_HIGH;
  props.rts = IO_SERIAL_HIGH;

  if (!IO_Serial_SetProperties (io, &props))
    return FALSE;

  while ((i < IO_SERIAL_PNPID_SIZE) &&
	 IO_Serial_Read (io, 200, 1, &(io->PnP_id[i])))
    i++;

  io->PnP_id_size = i;
  return TRUE;
}
