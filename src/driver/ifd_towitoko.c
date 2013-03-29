/*
    ifd_towitoko.c
    This module provides IFD handling functions.

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
#include <time.h>
#include <unistd.h>
#include "ifd_towitoko.h"
#include "io_serial.h"

/*
 * Not exported constants
 */

#define IFD_TOWITOKO_TIMEOUT             1000
#define IFD_TOWITOKO_DELAY               0
#define IFD_TOWITOKO_BAUDRATE            9600
#define IFD_TOWITOKO_PS                  15
#define IFD_TOWITOKO_MAX_TRANSMIT        255
#define IFD_TOWITOKO_ATR_TIMEOUT	 400
#define IFD_TOWITOKO_ATR_MIN_LENGTH      1
#define IFD_TOWITOKO_CLOCK_RATE          (372L * 9600L)

#define HI(a) 				(((a) & 0xff00) >> 8)
#define LO(a) 				((a) & 0x00ff)

/*
 * Not exported functions declaration
 */

static int IFD_Towitoko_PrepareCommand (IFD * ifd, BYTE * command, BYTE size);
static BYTE IFD_Towitoko_Checksum (BYTE * cmd, unsigned size, BYTE init);
static int IFD_Towitoko_GetReaderInfo (IFD * ifd);
static unsigned IFD_Towitoko_NumTrials (BYTE b);
static void IFD_Towitoko_Clear (IFD * ifd);

/*
 * Exported functions definition
 */

IFD *
IFD_Towitoko_New ()
{
  IFD *ifd;

  ifd = (IFD *) malloc (sizeof (IFD));

  if (ifd != NULL)
    IFD_Towitoko_Clear (ifd);

  return ifd;
}

void
IFD_Towitoko_Delete (IFD * ifd)
{
  free (ifd);
}

int
IFD_Towitoko_Init (IFD * ifd, IO_Serial * io, BYTE slot)
{
  IO_Serial_Properties props;
  int ret;

#ifdef DEBUG_IFD
  printf ("IFD: Initialicing slot number %d\n", slot);
#endif

  if ((slot != IFD_TOWITOKO_SLOT_A) && (slot != IFD_TOWITOKO_SLOT_B))
    return IFD_TOWITOKO_PARAM_ERROR;

  /* Default serial port settings */
  props.input_bitrate = IFD_TOWITOKO_BAUDRATE;
  props.output_bitrate = IFD_TOWITOKO_BAUDRATE;
  props.bits = 8;
  props.parity = IO_SERIAL_PARITY_EVEN;
  props.stopbits = 2;
  props.dtr = IO_SERIAL_HIGH;
  props.rts = IO_SERIAL_HIGH;

  if (!IO_Serial_SetProperties (io, &props))
    return IFD_TOWITOKO_IO_ERROR;

  /* Default ifd settings */
  ifd->io = io;
  ifd->slot = slot;
  ifd->type = IFD_TOWITOKO_UNKNOWN;

  ret = IFD_Towitoko_SetBaudrate (ifd, IFD_TOWITOKO_BAUDRATE);

  if (ret != IFD_TOWITOKO_OK)
    {
      IFD_Towitoko_Clear (ifd);
      return ret;
    }

  ret = IFD_Towitoko_SetParity (ifd, IFD_TOWITOKO_PARITY_EVEN);

  if (ret != IFD_TOWITOKO_OK)
    {
      IFD_Towitoko_Clear (ifd);
      return ret;
    }

  ret = IFD_Towitoko_GetReaderInfo (ifd);

  if (ret != IFD_TOWITOKO_OK)
    {
      IFD_Towitoko_Clear (ifd);
      return ret;
    }

  /* Kartenzwerg settings */
  if (ifd->type == IFD_TOWITOKO_KARTENZWERG)
    {
      props.input_bitrate = IFD_TOWITOKO_BAUDRATE;
      props.output_bitrate = IFD_TOWITOKO_BAUDRATE;
      props.bits = 8;
      props.dtr = IO_SERIAL_HIGH;
      props.rts = IO_SERIAL_HIGH;
      props.parity = IO_SERIAL_PARITY_NONE;
      props.stopbits = 1;

      if (!IO_Serial_SetProperties (ifd->io, &props))
	{
	  IFD_Towitoko_Clear (ifd);
	  return IFD_TOWITOKO_IO_ERROR;
	}
    }

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_Close (IFD * ifd)
{
  int ret;

#ifdef DEBUG_IFD
  printf ("IFD: Closing slot number %d\n", ifd->slot);
#endif

  ret = IFD_Towitoko_SetLED (ifd, IFD_TOWITOKO_LED_OFF);
  if (ret != IFD_TOWITOKO_OK)
    return ret;

  IFD_Towitoko_Clear (ifd);

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_SetBaudrate (IFD * ifd, unsigned long baudrate)
{
  BYTE status[1];
  BYTE buffer[6] = { 0x6E, 0x00, 0x00, 0x00, 0x08, 0x00};
  IO_Serial_Properties props;
#ifdef HAVE_NANOSLEEP
  struct timespec req_ts;
#endif

  if (IFD_Towitoko_GetMaxBaudrate (ifd) < baudrate)
    {
#ifdef DEBUG_IFD
      printf ("IFD: Tried to set unsupported baudrate: %lu", baudrate);
#endif
      return IFD_TOWITOKO_PARAM_ERROR;
    }
  
#ifdef DEBUG_IFD
  printf ("IFD: Setting baudrate to %lu\n", baudrate);
#endif

  /* Get current settings */
  if (!IO_Serial_GetProperties (ifd->io, &props))
    return IFD_TOWITOKO_IO_ERROR;

  if (props.output_bitrate == baudrate)
    return IFD_TOWITOKO_OK;
#if 1
  /* Convert baudrate to a quantum magnitude */
  if (baudrate <= 1200)
    {
      buffer[1] = 0x60;
      buffer[3] = 0x07;
    }

  else if (baudrate <= 2400)
    {
      buffer[1] = 0x2E;
      buffer[3] = 0x03;
    }

  else if (baudrate <= 4800)
    {
      buffer[1] = 0x17;
      buffer[3] = 0x05;
    }

  else if (baudrate <= 6975)
    {
      buffer[1] = 0x0F;
      buffer[3] = 0x01;
    }

  else if (baudrate <= 9600)
    {
      buffer[1] = 0x0B;
      buffer[3] = 0x02;
    }

  else if (baudrate <= 14400)
    {
      buffer[1] = 0x07;
      buffer[3] = 0x01;
    }

  else if (baudrate <= 19200)
    {
      buffer[1] = 0x05;
      buffer[3] = 0x02;
    } 

  else if (baudrate <= 28800)
    {
      buffer[1] = 0x03;
      buffer[3] = 0x00;
    }

  else if (baudrate <= 38400)
    {
      buffer[1] = 0x02;
      buffer[3] = 0x00;
    }

  else if (baudrate <= 57600)
    {
      buffer[1] = 0x01;
      buffer[3] = 0x00;
    }

  else if (baudrate <= 115200L)
    {
      buffer[1] = 0x80;
      buffer[3] = 0x00;
    }
  else
    return IFD_TOWITOKO_PARAM_ERROR;

  buffer[2] = buffer[1] ^ 0x5D;

  IFD_Towitoko_PrepareCommand (ifd, buffer, 6);

  /* Set  ifd baudrate requested */
  if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 6, buffer))
    return IFD_TOWITOKO_IO_ERROR;

  if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
    return IFD_TOWITOKO_IO_ERROR;

  if (status[0] != 0x01)
    return IFD_TOWITOKO_CHK_ERROR;
#endif
  /* Set serial device bitrate */
  props.output_bitrate = baudrate;
  props.input_bitrate = baudrate;

  if (!IO_Serial_SetProperties (ifd->io, &props))
    return IFD_TOWITOKO_IO_ERROR;

  /* Delay 150 ms until CHIPDRIVE is ready */
#ifdef HAVE_NANOSLEEP
  req_ts.tv_sec = 0;
  req_ts.tv_nsec = 150000000L;
  nanosleep (&req_ts, NULL);
#else
  usleep (150000L);
#endif

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_GetBaudrate (IFD * ifd, unsigned long *baudrate)
{
  IO_Serial_Properties props;

  /* Get current settings */
  if (!IO_Serial_GetProperties (ifd->io, &props))
    return IFD_TOWITOKO_IO_ERROR;

  (*baudrate) = props.output_bitrate;

  return IFD_TOWITOKO_OK;
}

extern int
IFD_Towitoko_SetParity (IFD * ifd, BYTE parity)
{
  BYTE buffer[5] = { 0x6F, 0x00, 0x6A, 0x0F, 0x00 };
  BYTE status[1];
  IO_Serial_Properties props;

  if (ifd->type == IFD_TOWITOKO_KARTENZWERG)
    return IFD_TOWITOKO_UNSUPPORTED;

#ifdef DEBUG_IFD
  printf ("IFD: Parity = %s\n",
	  parity == IFD_TOWITOKO_PARITY_ODD ? "Odd" :
	  parity == IFD_TOWITOKO_PARITY_EVEN ? "Even" : "Invalid");
#endif

  if ((parity != IFD_TOWITOKO_PARITY_EVEN) &&
      (parity != IFD_TOWITOKO_PARITY_ODD))
    return IFD_TOWITOKO_PARAM_ERROR;

  /* Get current settings */
  if (!IO_Serial_GetProperties (ifd->io, &props))
    return IFD_TOWITOKO_IO_ERROR;

  /* Set serial device parity to even */
  if (props.parity == IO_SERIAL_PARITY_ODD)
    {
      props.parity = IO_SERIAL_PARITY_EVEN;

      if (!IO_Serial_SetProperties (ifd->io, &props))
	return IFD_TOWITOKO_IO_ERROR;
    }

  /* Set ifd parity as requested */
  buffer[1] = parity;

  IFD_Towitoko_PrepareCommand (ifd, buffer, 5);
  
  if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 5, buffer))
    return IFD_TOWITOKO_IO_ERROR;

  if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
    return IFD_TOWITOKO_IO_ERROR;

  /*
     if (status[0] != 0x01)
     return IFD_TOWITOKO_CHK_ERROR;
   */

  /* Set serial device parity */
  if ((parity == IFD_TOWITOKO_PARITY_ODD) &&
      (props.parity == IO_SERIAL_PARITY_EVEN))
    {
      props.parity = IO_SERIAL_PARITY_ODD;

      if (!IO_Serial_SetProperties (ifd->io, &props))
	return IFD_TOWITOKO_IO_ERROR;
    }

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_SetLED (IFD * ifd, BYTE color)
{
  BYTE status[1];
  BYTE buffer[5] = { 0x6F, 0x00, 0x6A, 0x0F, 0x00 };

#ifdef DEBUG_IFD
  printf ("IFD: LED = %s\n",
	  color == IFD_TOWITOKO_LED_RED ? "Red" :
	  color == IFD_TOWITOKO_LED_GREEN ? "Green" :
	  color == IFD_TOWITOKO_LED_YELLOW ? "Yellow" :
	  color == IFD_TOWITOKO_LED_OFF ? "Off" : "Invalid Color");
#endif

  if ((color != IFD_TOWITOKO_LED_RED) &&
      (color != IFD_TOWITOKO_LED_GREEN) &&
      (color != IFD_TOWITOKO_LED_YELLOW) && (color != IFD_TOWITOKO_LED_OFF))
    return IFD_TOWITOKO_PARAM_ERROR;

  buffer[1] = color;

  IFD_Towitoko_PrepareCommand (ifd, buffer, 5);

  if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 5, buffer))
    return IFD_TOWITOKO_IO_ERROR;

  if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
    return IFD_TOWITOKO_IO_ERROR;

  if (status[0] != 0x01)
    return IFD_TOWITOKO_CHK_ERROR;

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_GetStatus (IFD * ifd, BYTE * result)
{
  BYTE buffer[2] = { 0x03, 0x07 };
  BYTE status[2];

  IFD_Towitoko_PrepareCommand (ifd, buffer, 2);

  if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 2, buffer))
    return IFD_TOWITOKO_IO_ERROR;

  if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 2, status))
    {
      /*
       * First read can exceed timeout if card has just
       * been inserted or removed
       */
      IFD_Towitoko_PrepareCommand (ifd, buffer, 2);
      
      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 2, buffer))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 2, status))
	return IFD_TOWITOKO_IO_ERROR;
    }

  (*result) = status[0];

#ifdef DEBUG_IFD
  printf ("IFD: Status = %s / %s\n",
  	  IFD_TOWITOKO_CARD(status[0])? "card": "no card",
  	  IFD_TOWITOKO_CHANGE(status[0])? "change": "no change");
#endif

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_ActivateICC (IFD * ifd)
{
  BYTE status[1];
  BYTE buffer[3] = { 0x60, 0x0F, 0x9C };

  IFD_Towitoko_PrepareCommand (ifd, buffer, 3);

#ifdef DEBUG_IFD
  printf ("IFD: Activating card\n");
#endif

  if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 3, buffer))
    return IFD_TOWITOKO_IO_ERROR;

  if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
    return IFD_TOWITOKO_IO_ERROR;

  if (status[0] != 0x01)
    return IFD_TOWITOKO_CHK_ERROR;

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_DeactivateICC (IFD * ifd)
{
  BYTE status[1];
  BYTE buffer[3] = { 0x61, 0x0F, 0x98 };

  IFD_Towitoko_PrepareCommand (ifd, buffer, 3);
  
#ifdef DEBUG_IFD
  printf ("IFD: Deactivating card\n");
#endif

  if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 3, buffer))
    return IFD_TOWITOKO_IO_ERROR;

  if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
    return IFD_TOWITOKO_IO_ERROR;

  if (status[0] != 0x01)
    return IFD_TOWITOKO_CHK_ERROR;

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_ResetAsyncICC (IFD * ifd, ATR ** atr)
{
  BYTE buffer1[5] = { 0x80, 0x6F, 0x00, 0x05, 0x76 };
  BYTE buffer2[5] = { 0xA0, 0x6F, 0x00, 0x05, 0x74 };
#ifndef IFD_TOWITOKO_STRICT_ATR_CHECK
  BYTE atr_buffer[ATR_MAX_SIZE];
  unsigned  atr_length;
#endif
  int i, parity, ret;

  if (ifd->type == IFD_TOWITOKO_KARTENZWERG)
    return IFD_TOWITOKO_UNSUPPORTED;

  buffer1[4] = IFD_Towitoko_Checksum (buffer1, 4, ifd->slot);
  buffer2[4] = IFD_Towitoko_Checksum (buffer2, 4, ifd->slot);

#ifdef DEBUG_IFD
  printf ("IFD: Resetting card:\n");
#endif

#ifndef IFD_TOWITOKO_CONVENTION_INVERSE
  parity = IFD_TOWITOKO_PARITY_EVEN;
#else
  parity = IFD_TOWITOKO_PARITY_ODD;
  ret = IFD_Towitoko_SetParity (ifd, parity);

  if (ret != IFD_TOWITOKO_OK)
    return ret;
#endif

  ret = IFD_TOWITOKO_IO_ERROR;

  do
    {
      for (i = 0; i < 2; i++)
	{
	  /* Try active-low reset */
	  if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 5, buffer2))
	    break;

	  (*atr) = ATR_New ();

#ifndef IFD_TOWITOKO_STRICT_ATR_CHECK
	  /* Read form input until it returns timeout */
	  for (atr_length = 0; atr_length < ATR_MAX_SIZE; atr_length++)
	    {
	      if (!IO_Serial_Read
		  (ifd->io, IFD_TOWITOKO_ATR_TIMEOUT, 1,
		   atr_buffer + atr_length))
		break;
	    }

	  if (atr_length >= IFD_TOWITOKO_ATR_MIN_LENGTH)
	    {
	      /* Try to parse the ATR */
	      ATR_InitFromArray ((*atr), atr_buffer, atr_length + 1);
	      ret = IFD_TOWITOKO_OK;
	      break;
	    }
#else
	  if (ATR_InitFromStream ((*atr), ifd->io, IFD_TOWITOKO_ATR_TIMEOUT) == ATR_OK)
	    {
	      ret = IFD_TOWITOKO_OK;
	      break;
	    }
#endif
	  ATR_Delete (*atr);
	  (*atr) = NULL;

	  /* Try active-high reset */
	  if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 5, buffer1))
	    break;

	  (*atr) = ATR_New ();

#ifndef IFD_TOWITOKO_STRICT_ATR_CHECK
	  /* Read form input until it returns timeout */
	  for (atr_length = 0; atr_length < ATR_MAX_SIZE; atr_length++)
	    {
	      if (!IO_Serial_Read
		  (ifd->io, IFD_TOWITOKO_ATR_TIMEOUT, 1,
		   atr_buffer + atr_length))
		break;
	    }

	  if (atr_length >= IFD_TOWITOKO_ATR_MIN_LENGTH)
	    {
	      /* Try to parse the ATR */
	      ATR_InitFromArray ((*atr), atr_buffer, atr_length + 1);
	      ret = IFD_TOWITOKO_OK;
	      break;
	    }
#else
	  if (ATR_InitFromStream ((*atr), ifd->io, IFD_TOWITOKO_ATR_TIMEOUT) == ATR_OK)
	    {
	      ret = IFD_TOWITOKO_OK;
	      break;
	    }
#endif
	  ATR_Delete (*atr);
	  (*atr) = NULL;
	}

      /* Succesfully retrive ATR */
      if (ret == IFD_TOWITOKO_OK)
	{
	  if (parity == IFD_TOWITOKO_PARITY_ODD)
	    {
	      parity = IFD_TOWITOKO_PARITY_EVEN;
	      ret = IFD_Towitoko_SetParity (ifd, parity);
	    }
	}

      /* Switch parity */
      else
	{
#ifndef IFD_TOWITOKO_CONVENTION_DIRECT
	  parity = ((parity == IFD_TOWITOKO_PARITY_EVEN) ?
		    IFD_TOWITOKO_PARITY_ODD : IFD_TOWITOKO_PARITY_EVEN);

	  IFD_Towitoko_SetParity (ifd, parity);
#endif
	}
    }
  while (parity != IFD_TOWITOKO_PARITY_EVEN);

  return ret;
}

int
IFD_Towitoko_Transmit (IFD * ifd, IFD_Timings * timings, unsigned size, BYTE * buffer)
{
  BYTE header[6] = {0x6F, 0x00, 0x05, 0x00, 0xFE, 0xF8};
  unsigned block_delay, char_delay, sent=0, to_send = 0;
  IO_Serial_Properties props;
  bool s = FALSE;

  if (ifd->type == IFD_TOWITOKO_KARTENZWERG)
    return IFD_TOWITOKO_UNSUPPORTED;

#ifdef DEBUG_IFD
  printf ("IFD: Transmit: ");
  for (sent = 0; sent < size; sent++)
    printf ("%X ", buffer[sent]);
  printf ("\n");
#endif

  /* Get current baudrate */
  if (!IO_Serial_GetProperties (ifd->io, &props))
    return IFD_TOWITOKO_IO_ERROR;

  s = (props.output_bitrate > IFD_TOWITOKO_BAUDRATE);
  
  /* Calculate delays */
  char_delay = IFD_TOWITOKO_DELAY + timings->char_delay;
  block_delay = IFD_TOWITOKO_DELAY + timings->block_delay;

  for (sent = 0; sent < size; sent = sent + to_send) 
    {
      /* Calculate number of bytes to send */
      to_send = MIN(size, IFD_TOWITOKO_MAX_TRANSMIT);

      /* Send  header */
      header[1] = (BYTE) to_send;

      IFD_Towitoko_PrepareCommand (ifd, header, 4);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, (s?6:4), header))
        return IFD_TOWITOKO_IO_ERROR;

      /* Send data */
      if ((sent == 0) && (block_delay != char_delay))
	{
          if (!IO_Serial_Write (ifd->io, block_delay, 1, buffer))
            return IFD_TOWITOKO_IO_ERROR;
	  
          if (!IO_Serial_Write (ifd->io, char_delay, to_send-1, buffer+1))
            return IFD_TOWITOKO_IO_ERROR;
        }
      else
	{
          if (!IO_Serial_Write (ifd->io, char_delay, to_send, buffer+sent))
            return IFD_TOWITOKO_IO_ERROR;
	}
    }

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_Receive (IFD * ifd, IFD_Timings * timings, unsigned size, BYTE * buffer)
{
  unsigned char_timeout, block_timeout;
#ifdef DEBUG_IFD
  int i;
#endif

  if (ifd->type == IFD_TOWITOKO_KARTENZWERG)
    return IFD_TOWITOKO_UNSUPPORTED;

  /* Calculate timeouts */
  char_timeout = IFD_TOWITOKO_TIMEOUT + timings->char_timeout;
  block_timeout = IFD_TOWITOKO_TIMEOUT + timings->block_timeout;

  if (block_timeout != char_timeout)
    {
      /* Read first byte using block timeout */
      if (!IO_Serial_Read (ifd->io, block_timeout, 1, buffer))
        return IFD_TOWITOKO_IO_ERROR;

      if (size > 1)
        {
          /* Read remaining data bytes using char timeout */
          if (!IO_Serial_Read (ifd->io, char_timeout, size - 1, buffer + 1))
	    return IFD_TOWITOKO_IO_ERROR;
        }
    }
  else
    {
      /* Read all data bytes with the same timeout */
      if (!IO_Serial_Read (ifd->io, char_timeout, size, buffer))
        return IFD_TOWITOKO_IO_ERROR;
    }

#ifdef DEBUG_IFD
  printf ("IFD: Receive: ");
  for (i = 0; i < size; i++)
    printf ("%X ", buffer[i]);
  printf ("\n");
#endif

  return IFD_TOWITOKO_OK;
}


int
IFD_Towitoko_Switch (IFD * ifd)
{
  IO_Serial_Properties props;
  BYTE buffer[1] = {0xF8};

  if (!IO_Serial_GetProperties (ifd->io, &props))
    return IFD_TOWITOKO_IO_ERROR;
  
  if (props.output_bitrate > IFD_TOWITOKO_BAUDRATE)
    {
      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 1, buffer))
        return IFD_TOWITOKO_IO_ERROR;
#ifdef DEBUG_IFD
      printf ("IFD: Switching\n");
#endif
    }

  return IFD_TOWITOKO_OK;
}

extern int IFD_Towitoko_ResetSyncICC (IFD * ifd, ATR_Sync ** atr)
{
  BYTE buffer[5] = {0x70, 0x80, 0x62, 0x0F, 0x00};
  BYTE atr_buffer[8], status[1];
  
  IFD_Towitoko_PrepareCommand (ifd, buffer, 5);

  if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 5, buffer))
    return IFD_TOWITOKO_IO_ERROR;

  if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
    return IFD_TOWITOKO_IO_ERROR;

  if (status[0] != 0x01)
    return IFD_TOWITOKO_CHK_ERROR;
    
  if (IFD_Towitoko_ReadBuffer (ifd, 8, atr_buffer) != IFD_TOWITOKO_OK)
    return IFD_TOWITOKO_IO_ERROR;

  if (atr_buffer[0] != 0xFF)
    {
      (*atr) = ATR_Sync_New ();

      if ((*atr) != NULL)
	ATR_Sync_Init ((*atr), atr_buffer, ATR_SYNC_SIZE);
    }
  
  else 
    (*atr) = NULL;
    
  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_SetReadAddress (IFD * ifd, int icc_type, unsigned short address)
{
  BYTE status[1];
  BYTE i2cShort[10] = { 0x7C, 0x64, 0x41, 0x00, 0x00, 0x64, 0x40, 0x00, 0x0F, 0x00 };
  BYTE i2cLong[11] = { 0x7C, 0x64, 0x42, 0xA0, 0x00, 0x00, 0x64, 0x40, 0xA1, 0x0F, 0x00 };
  BYTE w2[9] = { 0x70, 0x64, 0x42, 0x30, 0x00, 0x00, 0x65, 0x0F, 0x00 };
  BYTE w3[10] = { 0x70, 0xA0, 0x42, 0x00, 0x00, 0x00, 0x80, 0x50, 0x0F, 0x00 };

  if (icc_type == IFD_TOWITOKO_I2C_SHORT)
    {
#ifdef DEBUG_IFD
      printf ("IFD: I2C short set read address: %d\n", address);
#endif

      i2cShort[3] = (HI (address) << 1) | 0xA0;
      i2cShort[4] = LO (address);
      i2cShort[7] = (HI (address) << 1) | 0xA0 | 0x01;
  
      IFD_Towitoko_PrepareCommand (ifd, i2cShort, 10);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 10, i2cShort))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }
  else if (icc_type == IFD_TOWITOKO_I2C_LONG)
    {
#ifdef DEBUG_IFD
      printf ("IFD: I2C long set read address: %d\n", address);
#endif

      i2cLong[4] = HI (address);
      i2cLong[5] = LO (address);

      IFD_Towitoko_PrepareCommand (ifd, i2cLong, 11);
      
      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 11, i2cLong))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }
  else if (icc_type == IFD_TOWITOKO_2W)
    {
#ifdef DEBUG_IFD
      printf ("IFD: 2W set read address: %d\n", address);
#endif

      w2[4] = LO (address);

      IFD_Towitoko_PrepareCommand (ifd, w2, 9);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 9, w2))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }
  else if (icc_type == IFD_TOWITOKO_3W)
    {
#ifdef DEBUG_IFD
      printf ("IFD: 3W set read address: %d\n", address);
#endif

      w3[3] = (HI (address) << 6) | 0x0E;
      w3[4] = LO (address);

      IFD_Towitoko_PrepareCommand (ifd, w3, 10);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 10, w3))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }
  else
    return IFD_TOWITOKO_PARAM_ERROR;

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_SetWriteAddress (IFD * ifd, int icc_type, unsigned short address, BYTE pagemode)
{
  BYTE i2cShort1[10] = { 0x7C, 0x64, 0x41, 0xA0, 0x00, 0x64, 0x40, 0xA1, 0x0F, 0x36 };
  BYTE i2cShort2[3] = { 0x7E, 0x10, 0xDA };
  BYTE i2cShort3[8] = { 0x7E, 0x66, 0x6E, 0x00, 0x00, 0x10, 0x0F, 0x00 };
  BYTE i2cLong1[11] = { 0x7C, 0x64, 0x42, 0xA0, 0x00, 0x00, 0x64, 0x40, 0xA1, 0x0F, 0x0B };
  BYTE i2cLong2[3] = { 0x7E, 0x10, 0xDA };
  BYTE i2cLong3[8] = { 0x7F, 0x66, 0x6E, 0x00, 0x00, 0xA0, 0x0F, 0x00 };
  BYTE w2[7] = { 0x72, 0x6E, 0x00, 0x38, 0x03, 0x0F, 0x00 };
  BYTE w3[8] = { 0x73, 0x67, 0x6E, 0x00, 0x00, 0x02, 0x0F, 0x5F };
  BYTE status[2];

  if (icc_type == IFD_TOWITOKO_I2C_SHORT)
    {
#ifdef DEBUG_IFD
      printf ("IFD: I2C short set write address: %d\n", address);
#endif
      IFD_Towitoko_PrepareCommand (ifd, i2cShort1, 10);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 10, i2cShort1))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      IFD_Towitoko_PrepareCommand (ifd, i2cShort2, 3);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 3, i2cShort2))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 2, status))
	return IFD_TOWITOKO_IO_ERROR;

      i2cShort3[3] = LO (address);
      i2cShort3[4] = (HI (address) << 1) | 0xA0;
      i2cShort3[5] = pagemode;

      IFD_Towitoko_PrepareCommand (ifd, i2cShort3, 8);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 8, i2cShort3))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }
  else if (icc_type == IFD_TOWITOKO_I2C_LONG)
    {
#ifdef DEBUG_IFD
      printf ("IFD: I2C long set write address: %d\n", address);
#endif
      IFD_Towitoko_PrepareCommand (ifd, i2cLong1, 11);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 11, i2cLong1))
	return IFD_TOWITOKO_CHK_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      IFD_Towitoko_PrepareCommand (ifd, i2cLong2, 3);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 3, i2cLong2))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 2, status))
	return IFD_TOWITOKO_IO_ERROR;

      i2cLong3[3] = LO (address);
      i2cLong3[4] = HI (address);

      IFD_Towitoko_PrepareCommand (ifd, i2cLong3, 8);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 8, i2cLong3))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }
  else if (icc_type == IFD_TOWITOKO_2W)
    {
#ifdef DEBUG_IFD
      printf ("IFD: 2W set write address: %d\n", address);
#endif

      w2[2] = LO (address);

      IFD_Towitoko_PrepareCommand (ifd, w2, 7);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 7, w2))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }
  else if (icc_type == IFD_TOWITOKO_3W)
    {
#ifdef DEBUG_IFD
      printf ("IFD: 3W set write address: %d\n", address);
#endif

      w3[3] = LO (address);
      w3[4] = (HI (address) << 6) | 0x33;

      IFD_Towitoko_PrepareCommand (ifd, w3, 8);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 8, w3))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }
  else
    return IFD_TOWITOKO_PARAM_ERROR;

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_ReadBuffer (IFD * ifd, unsigned length, BYTE * data)
{
  BYTE buffer[2], status[1];
  unsigned blocks_length, pointer;

  buffer[0] = ((BYTE) (IFD_TOWITOKO_PS - 1)) | 0x10;
  blocks_length = (length / IFD_TOWITOKO_PS) * IFD_TOWITOKO_PS;

  for (pointer = 0; pointer < blocks_length; pointer += IFD_TOWITOKO_PS)
    {
      IFD_Towitoko_PrepareCommand (ifd, buffer, 2);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 2, buffer))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT,
			   IFD_TOWITOKO_PS, data + pointer))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;
    }

  if ((length % IFD_TOWITOKO_PS) != 0)
    {
      buffer[0] = ((BYTE) ((length % IFD_TOWITOKO_PS) - 1)) | 0x10;

      IFD_Towitoko_PrepareCommand (ifd, buffer, 2);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 2, buffer))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT,
			   length % IFD_TOWITOKO_PS, data + pointer))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;
    }

#ifdef DEBUG_IFD
  printf ("IFD: Read Memory: ");
  for (pointer = 0; pointer < length; pointer++)
    printf ("%X ", data[pointer]);
  printf ("\n");
#endif

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_WriteBuffer (IFD * ifd, unsigned length, BYTE * data)
{
  BYTE buffer[IFD_TOWITOKO_PS + 2], status[1];
  unsigned blocks_length, remaining_length, pointer;

  buffer[0] = 0x4E;
  blocks_length = (length / IFD_TOWITOKO_PS) * IFD_TOWITOKO_PS;

  for (pointer = 0; pointer < blocks_length; pointer += IFD_TOWITOKO_PS)
    {
      memcpy (buffer + 1, data + pointer, IFD_TOWITOKO_PS);
      
      IFD_Towitoko_PrepareCommand (ifd, buffer, IFD_TOWITOKO_PS + 2);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, IFD_TOWITOKO_PS + 2, buffer))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }

  remaining_length = length % IFD_TOWITOKO_PS;

  if (remaining_length != 0)
    {
      buffer[0] = ((BYTE) ((remaining_length) - 1)) | 0x40;
      memcpy (buffer + 1, data + pointer, remaining_length);
      buffer[remaining_length + 1] = 0x0F;
      
      IFD_Towitoko_PrepareCommand (ifd, buffer, remaining_length + 3);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, remaining_length + 3, buffer))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }

#ifdef DEBUG_IFD
  printf ("IFD: Write Memory: ");
  for (pointer = 0; pointer < length; pointer++)
    printf ("%X ", data[pointer]);
  printf ("\n");
#endif

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_ReadErrorCounter (IFD * ifd, int icc_type, unsigned *trials)
{
  BYTE w21[9] = { 0x70, 0x64, 0x42, 0x31, 0x00, 0x00, 0x65, 0x0F, 0x80 };
  BYTE w22[2] = { 0x13, 0x27 };
  BYTE w31[10] = { 0x70, 0xA0, 0x42, 0xCE, 0xFD, 0xFD, 0x80, 0x50, 0x0F, 0x17 };
  BYTE w32[2] = { 0x10, 0x21 };
  BYTE status[5];

  if (icc_type == IFD_TOWITOKO_2W)
    {
      IFD_Towitoko_PrepareCommand (ifd, w21, 9);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 9, w21))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      IFD_Towitoko_PrepareCommand (ifd, w22, 2);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 2, w22))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 5, status))
	return IFD_TOWITOKO_IO_ERROR;

      (*trials) = IFD_Towitoko_NumTrials (status[0]);

#ifdef DEBUG_IFD
      printf ("IFD: Read Error Counter 2W: %d\n", *trials);
#endif
    }
  else if (icc_type == IFD_TOWITOKO_3W)
    {
      IFD_Towitoko_PrepareCommand (ifd, w31, 10);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 10, w31))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      IFD_Towitoko_PrepareCommand (ifd, w32, 2);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 2, w32))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 2, status))
	return IFD_TOWITOKO_IO_ERROR;

      (*trials) = IFD_Towitoko_NumTrials (status[0]);

#ifdef DEBUG_IFD
      printf ("IFD: Read Error Counter 3W: %d\n", *trials);
#endif
    }

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_EnterPin (IFD * ifd, int icc_type, BYTE * pin, unsigned trial)
{
  BYTE w21[7] = { 0x72, 0x6E, 0x00, 0x39, 0x03, 0x0F, 0xB5 };
  BYTE w22[4] = { 0x40, 0x00, 0x0F, 0x00 };
  BYTE w23[6] = { 0x42, 0x00, 0x00, 0x00, 0x0F, 0x00 };
  BYTE w31[8] = { 0x73, 0x67, 0x6E, 0xFD, 0xF2, 0x02, 0x0F, 0x8C };
  BYTE w32[4] = { 0x40, 0x00, 0x0F, 0x00 };
  BYTE status[1];

  if (icc_type == IFD_TOWITOKO_2W)
    {
#ifdef DEBUG_IFD
      printf ("IFD: 2W enter pin: %X %X %X\n", pin[0], pin[1], pin[2]);
#endif
      IFD_Towitoko_PrepareCommand (ifd, w21, 7);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 7, w21))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w22[1] = (trial == 3) ? 0x06 : (trial == 2) ? 0x04 : 0x00;

      IFD_Towitoko_PrepareCommand (ifd, w22, 4);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 4, w22))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w21[2] = 0x01;
      w21[3] = 0x33;

      IFD_Towitoko_PrepareCommand (ifd, w21, 7);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 7, w21))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      memcpy (w23 + 1, pin, IFD_TOWITOKO_PIN_SIZE);

      IFD_Towitoko_PrepareCommand (ifd, w23, 6);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 6, w23))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      w21[2] = 0x00;
      w21[3] = 0x39;

      IFD_Towitoko_PrepareCommand (ifd, w21, 7);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 7, w21))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w22[1] = 0xFF;

      IFD_Towitoko_PrepareCommand (ifd, w22, 4);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 4, w22))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }

  else if (icc_type == IFD_TOWITOKO_3W)
    {
#ifdef DEBUG_IFD
      printf ("IFD: 3W enter pin: %X %X\n", pin[0], pin[1]);
#endif
      IFD_Towitoko_PrepareCommand (ifd, w31, 8);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 8, w31))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w32[1] = (trial == 8) ? 0xFE :
	(trial == 7) ? 0xFC :
	(trial == 6) ? 0xF8 :
	(trial == 5) ? 0xF0 :
	(trial == 4) ? 0xE0 :
	(trial == 3) ? 0xC0 : (trial == 2) ? 0x80 : 0x00;

      IFD_Towitoko_PrepareCommand (ifd, w32, 4);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 4, w32))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w31[3] = 0xFE;
      w31[4] = 0xCD;

      IFD_Towitoko_PrepareCommand (ifd, w31, 8);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 8, w31))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w32[1] = pin[0];

      IFD_Towitoko_PrepareCommand (ifd, w32, 4);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 4, w32))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w31[3] = 0xFF;
      IFD_Towitoko_PrepareCommand (ifd, w31, 8);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 8, w31))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w32[1] = pin[1];

      IFD_Towitoko_PrepareCommand (ifd, w32, 4);
      
      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 4, w32))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w31[3] = 0xFD;
      w31[4] = 0xF3;

      IFD_Towitoko_PrepareCommand (ifd, w31, 8);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 8, w31))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w32[1] = 0xFF;

      IFD_Towitoko_PrepareCommand (ifd, w32, 4);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 4, w32))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }
  else
    return IFD_TOWITOKO_PARAM_ERROR;

  return IFD_TOWITOKO_OK;
}

int
IFD_Towitoko_ChangePin (IFD * ifd, int icc_type, BYTE * pin)
{
  BYTE w21[7] = { 0x72, 0x6E, 0x01, 0x39, 0x03, 0x0F, 0xA5 };
  BYTE w22[6] = { 0x42, 0x00, 0x00, 0x00, 0x0F, 0x00 };
  BYTE w31[8] = { 0x73, 0x67, 0x6E, 0xFE, 0xF3, 0x02, 0x0F, 0xB4 };
  BYTE w32[4] = { 0x40, 0x00, 0x0F, 0x00 };
  BYTE status[1];

  if (icc_type == IFD_TOWITOKO_2W)
    {
#ifdef DEBUG_IFD
      printf ("IFD: 2W change pin: %X %X %X\n", pin[0], pin[1], pin[2]);
#endif
      IFD_Towitoko_PrepareCommand (ifd, w21, 7);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 7, w21))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w22[1] = pin[0];
      w22[2] = pin[1];
      w22[3] = pin[2];

      IFD_Towitoko_PrepareCommand (ifd, w22, 6);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 6, w22))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }

  else if (icc_type == IFD_TOWITOKO_3W)
    {
#ifdef DEBUG_IFD
      printf ("IFD: 3W change pin: %X %X\n", pin[0], pin[1]);
#endif
      IFD_Towitoko_PrepareCommand (ifd, w31, 8);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 8, w31))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w32[1] = pin[0];

      IFD_Towitoko_PrepareCommand (ifd, w32, 4);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 4, w32))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w31[3] = 0xFF;

      IFD_Towitoko_PrepareCommand (ifd, w31, 8);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 8, w31))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;

      w32[1] = pin[1];
      
      IFD_Towitoko_PrepareCommand (ifd, w32, 4);

      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 4, w32))
	return IFD_TOWITOKO_IO_ERROR;

      if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 1, status))
	return IFD_TOWITOKO_IO_ERROR;

      if (status[0] != 0x01)
	return IFD_TOWITOKO_CHK_ERROR;
    }

  else
    return IFD_TOWITOKO_PARAM_ERROR;

  return IFD_TOWITOKO_OK;
}

BYTE
IFD_Towitoko_GetType (IFD * ifd)
{
  return ifd->type;
}

void
IFD_Towitoko_GetDescription (IFD * ifd, BYTE * desc, unsigned length)
{
	char buffer[3];

	if (ifd->type == IFD_TOWITOKO_CHIPDRIVE_EXT_II)
		memcpy (desc,"CE2",MIN(length,3));

	else if  (ifd->type == IFD_TOWITOKO_CHIPDRIVE_EXT_I)
		memcpy (desc,"CE1",MIN(length,3));

	else if (ifd->type == IFD_TOWITOKO_CHIPDRIVE_INT)
		memcpy (desc,"CDI",MIN(length,3));

	else if (ifd->type == IFD_TOWITOKO_CHIPDRIVE_MICRO)
		memcpy (desc,"CDM",MIN(length,3));

	else if (ifd->type == IFD_TOWITOKO_KARTENZWERG_II) 
		memcpy (desc,"KZ2",MIN(length,3));

	else if (ifd->type == IFD_TOWITOKO_KARTENZWERG)
		memcpy (desc,"KZ1",MIN(length,3));

	else 
		memcpy (desc,"UNK",MIN(length,3));
	
	snprintf (buffer, 3, "%02X", ifd->firmware);

	if (length > 3)
		memcpy (desc+3, buffer, MIN(length-3,2));
}

BYTE
IFD_Towitoko_GetFirmware (IFD * ifd)
{
  return ifd->firmware;
}

BYTE
IFD_Towitoko_GetSlot (IFD * ifd)
{
  return ifd->slot;
}

unsigned
IFD_Towitoko_GetNumSlots (IFD * ifd)
{
  if (ifd->type == IFD_TOWITOKO_CHIPDRIVE_EXT_II)
    return 2;

  return 1;
}

unsigned long
IFD_Towitoko_GetClockRate (IFD * ifd)
{
  return IFD_TOWITOKO_CLOCK_RATE;
}

unsigned long 
IFD_Towitoko_GetMaxBaudrate (IFD * ifd)
{
  return 115200L;
}

/*
 * Not exported funcions definition
 */

static int
IFD_Towitoko_PrepareCommand (IFD * ifd, BYTE * command, BYTE size)
{
  IO_Serial_Properties props;
  BYTE buffer[1], initial;

  if (!IO_Serial_GetProperties (ifd->io, &props))
    return IFD_TOWITOKO_IO_ERROR;

  if (props.output_bitrate >= 115200L)
    {
      buffer[0] = size - 1;
      
      if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 1, buffer))
        return IFD_TOWITOKO_IO_ERROR;

      initial = IFD_Towitoko_Checksum(buffer, 1, ifd->slot);
    }

  else
    initial = ifd->slot;

  command[size-1] = IFD_Towitoko_Checksum(command, size-1, initial);

  return IFD_TOWITOKO_OK;
}

static BYTE
IFD_Towitoko_Checksum (BYTE * command, unsigned size, BYTE initial)
{
  BYTE checksum, x7;
  unsigned i;

  checksum = initial;
  for (i = 0; i < size; i++)
    {
      checksum = checksum ^ command[i];
      x7 = (checksum & 0x80) >> 7;
      checksum = checksum << 1;
      checksum = !x7 == 0x01 ? checksum | 0x01 : checksum & 0xFE;
    }

  return checksum;
}

static int
IFD_Towitoko_GetReaderInfo (IFD * ifd)
{
  BYTE status[3];
  BYTE buffer[2] = { 0x00, 0x01};

  buffer[1] = IFD_Towitoko_Checksum (buffer, 1, ifd->slot);
  
  if (!IO_Serial_Write (ifd->io, IFD_TOWITOKO_DELAY, 2, buffer))
    return IFD_TOWITOKO_IO_ERROR;

  if (!IO_Serial_Read (ifd->io, IFD_TOWITOKO_TIMEOUT, 3, status))
    return IFD_TOWITOKO_IO_ERROR;

  ifd->type = status[0];
  ifd->firmware = status[1];

#ifdef DEBUG_IFD
  printf ("IFD: Reader type = %s\n",
	  status[0] == IFD_TOWITOKO_CHIPDRIVE_EXT_II ? "Chipdrive Extern II" :
	  status[0] == IFD_TOWITOKO_CHIPDRIVE_EXT_I ? "Chipdrive Extern I" :
	  status[0] == IFD_TOWITOKO_CHIPDRIVE_INT ? "Chipdrive Intern" :
	  status[0] == IFD_TOWITOKO_CHIPDRIVE_MICRO ? "Chipdrive Micro" :
	  status[0] == IFD_TOWITOKO_KARTENZWERG_II ? "Kartenzwerg II" :
	  status[0] == IFD_TOWITOKO_KARTENZWERG ? "Kartenzwerg" : "Unknown");
#endif
  
  return IFD_TOWITOKO_OK;
}

static unsigned
IFD_Towitoko_NumTrials (BYTE b)
{
  unsigned i, count = 0;

  for (i = 0; i < 8; i++)
    {
      count += ((b & 0x01) == 0x01) ? 1 : 0;
      b >>= 1;
    }

  return count;
}

static void
IFD_Towitoko_Clear (IFD * ifd)
{
  ifd->io = NULL;
  ifd->slot = 0x00;
  ifd->type = 0x00;
  ifd->firmware = 0x00;
}
