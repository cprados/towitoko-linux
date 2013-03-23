/*
    icc_sync.c
    Synchronous ICC's handling functions

    This file is part of the Unix driver for Towitoko smartcard readers
    Copyright (C) 2000 Carlos Prados <cprados@yahoo.com>

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
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "icc_sync.h"

/*
 * Not exported constants definition
 */

#define ICC_SYNC_MAX_MEMORY		8192
#define ICC_SYNC_MAX_TRANSMIT		256	/*  This is the maximum
						   to increase performance
						   without bypassing
						   address counter */
#define ICC_SYNC_I2C_MAX_RETRIES	2
#define ICC_SYNC_I2C_RETRY_TRIGGER	1
#define ICC_SYNC_BAUDRATE		115200L


/*
 * Not exported macros definition
 */

#define ICC_SYNC_NEEDS_PIN(icc)		(((icc)->type == ICC_SYNC_2W || \
					(icc)->type == ICC_SYNC_3W) && \
					(icc)->pin_needed)
#define ICC_SYNC_NEEDS_ACTIVATE(icc)	(!(icc)->active)
#define ICC_SYNC_NEEDS_DEACTIVATE(icc)	((icc)->type != ICC_SYNC_3W && \
					(icc)->active)

/*
 * Not exported functions declaration
 */

static int ICC_Sync_ProbeCardType (ICC_Sync * icc);
static int ICC_Sync_ProbeMemoryLength (ICC_Sync * icc);
static int ICC_Sync_ProbePagemode (ICC_Sync * icc);
static ATR_Sync * ICC_Sync_CreateAtr(ICC_Sync * icc);
static void ICC_Sync_Clear (ICC_Sync * icc);

/*
 * Exported functions definition
 */

ICC_Sync *
ICC_Sync_New (void)
{
  ICC_Sync *icc;

  /* Allocate memory */
  icc = (ICC_Sync *) malloc (sizeof (ICC_Sync));

  if (icc != NULL)
    ICC_Sync_Clear (icc);

  return icc;
}

void
ICC_Sync_Delete (ICC_Sync * icc)
{
  free (icc);
}

int
ICC_Sync_Init (ICC_Sync * icc, IFD * ifd)
{
#ifndef ICC_TYPE_ASYNC
  int ret;
  
  /* LED Red */
  if (IFD_Towitoko_SetLED (ifd, IFD_TOWITOKO_LED_RED) != IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;
  
  /* Initialize baudrate*/
  if (IFD_Towitoko_SetBaudrate (ifd, ICC_SYNC_BAUDRATE) != IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  /* Activate ICC */
  if (IFD_Towitoko_ActivateICC (ifd) != IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  /* Reset ICC */
  if (IFD_Towitoko_ResetSyncICC (ifd, &(icc->atr)) != IFD_TOWITOKO_OK)
    {
      icc->atr = NULL;
      return ICC_SYNC_IFD_ERROR;
    }

  /* Intialice member variables */
  icc->active = TRUE;
  icc->baudrate = ICC_SYNC_BAUDRATE;
  icc->ifd = ifd;
  icc->pin_ok = FALSE;
  icc->pin_needed = TRUE;

  /* Probe Card type  */
  ret = ICC_Sync_ProbeCardType (icc);
  if (ret != ICC_SYNC_OK)
    {
      ICC_Sync_Clear (icc);
      return ret;
    }

  /* Probe memory length */
  ret = ICC_Sync_ProbeMemoryLength (icc);
  if (ret != ICC_SYNC_OK)
    {
      ICC_Sync_Clear (icc);
      return ret;
    }

  /* Probe pagemode */
  ret = ICC_Sync_ProbePagemode (icc);
  if (ret != ICC_SYNC_OK)
    {
      ICC_Sync_Clear (icc);
      return ret;
    }

  /* Create fake ATR if card haven't one */
  if (icc->atr == NULL)
    icc->atr = ICC_Sync_CreateAtr (icc);

  /* LED Green */
  if (IFD_Towitoko_SetLED (ifd, IFD_TOWITOKO_LED_GREEN) != IFD_TOWITOKO_OK)
    {
      ICC_Sync_Clear (icc);
      return ICC_SYNC_IFD_ERROR;
    }

  if (ICC_SYNC_NEEDS_DEACTIVATE (icc))
    {
      if (IFD_Towitoko_DeactivateICC (icc->ifd) != IFD_TOWITOKO_OK)
	{
	  ICC_Sync_Clear (icc);
	  return ICC_SYNC_IFD_ERROR;
	}

      icc->active = FALSE;
    }

  return ICC_SYNC_OK;
#else
  return ICC_SYNC_DETECT_ERROR;
#endif
}

int
ICC_Sync_Close (ICC_Sync * icc)
{
  /* Deactivate ICC */
  if (IFD_Towitoko_DeactivateICC (icc->ifd) != IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  /* LED Off */
  if (IFD_Towitoko_SetLED (icc->ifd, IFD_TOWITOKO_LED_OFF) != IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  if (icc->atr != NULL)
    ATR_Sync_Delete (icc->atr);

  ICC_Sync_Clear (icc);

  return ICC_SYNC_OK;
}

int
ICC_Sync_Read (ICC_Sync * icc, unsigned short address, unsigned length,
	       BYTE * data)
{
  /* Re-activate card before reset address counter */
  if (ICC_SYNC_NEEDS_ACTIVATE (icc))
    {
      if (IFD_Towitoko_ActivateICC (icc->ifd) != IFD_TOWITOKO_OK)
	return ICC_SYNC_IFD_ERROR;

      icc->active = TRUE;
    }

  /* Read access */
  if (IFD_Towitoko_SetReadAddress (icc->ifd, icc->type, address) != IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  if (IFD_Towitoko_ReadBuffer (icc->ifd, length, data) != IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  if (ICC_SYNC_NEEDS_DEACTIVATE (icc))
    {
      if (IFD_Towitoko_DeactivateICC (icc->ifd) != IFD_TOWITOKO_OK)
	return ICC_SYNC_IFD_ERROR;

      icc->pin_needed = TRUE;
      icc->active = FALSE;
    }

  return ICC_SYNC_OK;
}

int
ICC_Sync_Write (ICC_Sync * icc, unsigned short address, unsigned length,
		BYTE * data)
{
  BYTE buffer[ICC_SYNC_MAX_TRANSMIT], mask;
  unsigned written, to_write, retries, max_retries, trials;
  int ret;

  if (length > ICC_SYNC_I2C_RETRY_TRIGGER)
    {
      /* At least one retry is needed for writting I2C */
      if ((icc->type == ICC_SYNC_I2C_SHORT)
	  || icc->type == (ICC_SYNC_I2C_LONG))
	max_retries = ICC_SYNC_I2C_MAX_RETRIES;
      else
	max_retries = 1;
    }
  else
    max_retries = 1;

  /* 
   * Divide data into smaller buffers to:
   *    - Don't bypass low byte of write address counter
   *    - Compare data written and read to see if memory is Read Only
   *    - Retry when first writting fails with I2C cards
   */
  for (written = 0; written < length; written += to_write)
    {
      /* See how many bytes can be written to the current page */
      mask = icc->pagemode - 0x01;
      
      to_write =
      MIN (MIN (ICC_SYNC_MAX_TRANSMIT, length - written),
     (((address + written) | mask) + 1) - (address + written));

      /* Repeat Until to_write bytes are written or max_retries are reached */
      retries = 0;
      do
	{
	  /* Re-activate card before reset address */
	  if (ICC_SYNC_NEEDS_ACTIVATE (icc))
	    {
	      if (IFD_Towitoko_ActivateICC (icc->ifd) != IFD_TOWITOKO_OK)
		return ICC_SYNC_IFD_ERROR;

	      icc->active = TRUE;

	      /* 2W cards needs to re-enter PIN */
	      if (icc->pin_ok && ICC_SYNC_NEEDS_PIN (icc))
		{
		  ret = ICC_Sync_EnterPin (icc, icc->pin, &trials);
		  if (ret != ICC_SYNC_OK)
		    return ret;
		}
	    }

	  /* Write access */
	  if (IFD_Towitoko_SetWriteAddress (icc->ifd, icc->type,
					    address + written,
					    icc->pagemode) != IFD_TOWITOKO_OK)
	    return ICC_SYNC_IFD_ERROR;

	  if (IFD_Towitoko_WriteBuffer (icc->ifd, to_write, data + written) !=
	      IFD_TOWITOKO_OK)
	    return ICC_SYNC_IFD_ERROR;

	  if (ICC_SYNC_NEEDS_DEACTIVATE (icc))
	    {
	      if (IFD_Towitoko_DeactivateICC (icc->ifd) != IFD_TOWITOKO_OK)
		return ICC_SYNC_IFD_ERROR;

	      icc->pin_needed = TRUE;
	      icc->active = FALSE;
	    }

	  /* See if the buffer has been written */
	  ret = ICC_Sync_Read (icc, address + written, to_write, buffer);

	  if (ret != ICC_SYNC_OK)
	    return ret;
	}
      while ((memcmp (data + written, buffer, to_write) != 0) &&
	     ((++retries) < max_retries));

      if (retries == max_retries)
	return ICC_SYNC_RO_ERROR;

      if (IFD_Towitoko_GetType (icc->ifd) == IFD_TOWITOKO_CHIPDRIVE_INT)
	{
#ifdef HAVE_NANOSLEEP
	  struct timespec req_ts;

	  req_ts.tv_sec = 0;
	  req_ts.tv_nsec = 90000000;
	  nanosleep (&req_ts, NULL);
#else
	  usleep (90000);
#endif
	}
    }

  return ICC_SYNC_OK;
}

int
ICC_Sync_EnterPin (ICC_Sync * icc, BYTE * pin, unsigned *trials)
{
  unsigned new_trials;

  /* See if card needs PIN */
  if (icc->type == ICC_SYNC_I2C_LONG || icc->type == ICC_SYNC_I2C_SHORT)
    return ICC_SYNC_OK;

  /* 2W ICC's needs to be re-activated before entering PIN */
  if (ICC_SYNC_NEEDS_ACTIVATE (icc))
    {
      if (IFD_Towitoko_ActivateICC (icc->ifd) != IFD_TOWITOKO_OK)
	return ICC_SYNC_IFD_ERROR;

      icc->active = TRUE;
    }

  /* Get error counter and stop if it is exahusted */
  if (IFD_Towitoko_ReadErrorCounter (icc->ifd, icc->type, trials) !=
      IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  if ((*trials) == 0)
    return ICC_SYNC_BLOCKED_ERROR;

  /* Enter PIN */
  if (IFD_Towitoko_EnterPin (icc->ifd, icc->type, pin, (*trials)) !=
      IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  /* See if the error counter has maximum value */
  if (IFD_Towitoko_ReadErrorCounter (icc->ifd, icc->type, &new_trials) !=
      IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  if (new_trials < (*trials))
    {
      icc->pin_ok = FALSE;
      (*trials) = new_trials;
      return ICC_SYNC_PIN_ERROR;
    }

  memcpy (icc->pin, pin, ICC_SYNC_PIN_SIZE);
  icc->pin_ok = TRUE;
  icc->pin_needed = FALSE;

  (*trials) = new_trials;
  return ICC_SYNC_OK;
}

int
ICC_Sync_ChangePin (ICC_Sync * icc, BYTE * pin)
{
  unsigned trials;
  int ret;

  /* See if card needs PIN */
  if (icc->type == ICC_SYNC_I2C_LONG || icc->type == ICC_SYNC_I2C_SHORT)
    return ICC_SYNC_OK;

  /* Re-activate card */
  if (ICC_SYNC_NEEDS_ACTIVATE (icc))
    {
      if (IFD_Towitoko_ActivateICC (icc->ifd) != IFD_TOWITOKO_OK)
	return ICC_SYNC_IFD_ERROR;

      icc->active = TRUE;

      /* 2W cards needs to re-enter PIN */
      if (icc->pin_ok && ICC_SYNC_NEEDS_PIN (icc))
	{
	  ret = ICC_Sync_EnterPin (icc, icc->pin, &trials);
	  if (ret != ICC_SYNC_OK)
	    return ret;
	}
    }

  /* Change PIN */
  if (IFD_Towitoko_ChangePin (icc->ifd, icc->type, pin) != IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  memcpy (icc->pin, pin, ICC_SYNC_PIN_SIZE);
  icc->pin_ok = TRUE;
  icc->pin_needed = FALSE;

  return ICC_SYNC_OK;
}

int
ICC_Sync_BeginTransmission (ICC_Sync * icc)
{
  /* Setup baudrate for this ICC */
  if (IFD_Towitoko_SetBaudrate (icc->ifd, ICC_SYNC_BAUDRATE) !=
      IFD_TOWITOKO_OK)
    return ICC_SYNC_IFD_ERROR;

  return ICC_SYNC_OK;
}

int
ICC_Sync_SetBaudrate (ICC_Sync * icc, unsigned long baudrate)
{
  icc->baudrate = baudrate;
  return ICC_SYNC_OK;
}

int
ICC_Sync_GetBaudrate (ICC_Sync * icc, unsigned long *baudrate)
{
  (*baudrate) = icc->baudrate;

  return ICC_SYNC_OK;
}

unsigned
ICC_Sync_GetLength (ICC_Sync * icc)
{
  return icc->length;
}

int
ICC_Sync_GetType (ICC_Sync * icc)
{
  return icc->type;
}

BYTE
ICC_Sync_GetPagemode (ICC_Sync * icc)
{
  return icc->pagemode;
}

IFD *
ICC_Sync_GetIFD (ICC_Sync * icc)
{
  return icc->ifd;
}

ATR_Sync *
ICC_Sync_GetAtr (ICC_Sync * icc)
{
  return icc->atr;
}

/*
 * Not exported functions definition
 */

static int
ICC_Sync_ProbeCardType (ICC_Sync * icc)
{
#ifndef ICC_SYNC_MEMORY_TYPE
  BYTE protocol, status[1], orig[1], modif[1];
  int ret;
  
  if (icc->atr != NULL)
    {
      ret = ICC_SYNC_OK;

      protocol = ATR_Sync_GetProtocolType(icc->atr);
       
      if (protocol == ATR_SYNC_PROTOCOL_TYPE_3W)
        icc->type = ICC_SYNC_3W;
        
      else if (protocol == ATR_SYNC_PROTOCOL_TYPE_2W)
        icc->type = ICC_SYNC_2W;
        
      else if (protocol == ATR_SYNC_PROTOCOL_TYPE_SDA)
        icc->type = ICC_SYNC_I2C_SHORT;
        
      else
      {
        ret = ICC_SYNC_DETECT_ERROR;
#ifdef DEBUG_ICC
	printf ("ICC: Detected synchronous card with unknown ATR\n");
#endif 
      }
    }
    
  else
    { 
      /* Check for I2C (short or long) */
      IFD_Towitoko_SetReadAddress (icc->ifd,  IFD_TOWITOKO_I2C_SHORT, 0);
      IFD_Towitoko_GetStatus  (icc->ifd, status);
      IFD_Towitoko_DeactivateICC (icc->ifd);

      if ((status[0] & 0x10) != 0x10)
        {
	  /* Check for I2CX card */
	  icc->type = IFD_TOWITOKO_I2C_SHORT;

          ICC_Sync_Read (icc, 0, 1, orig);

	  if (orig[0] == 0xFF)
	    modif[0] = 0x01;
	  else if (orig[0] == 0x00)
	    modif[0] = 0xFE;
	  else
	    modif[0] = ~orig[0];

	  if (ICC_Sync_Write (icc, 0, 1, modif) == ICC_SYNC_OK)
	    ICC_Sync_Write (icc, 0, 1, orig);
	  else
            icc->type = IFD_TOWITOKO_I2C_LONG;
	
          IFD_Towitoko_ActivateICC (icc->ifd);
          
          ret = ICC_SYNC_OK;
        }
      
      else
        ret = ICC_SYNC_DETECT_ERROR;
    }

#ifdef DEBUG_ICC
  if (ret == ICC_SYNC_OK)
    printf ("ICC: Detected %s memory card\n", 
  	  icc->type == 0? "I2C short": 
  	  icc->type == 1? "I2C long": 
  	  icc->type == 2? "2-wire bus protocol":
  	  icc->type == 3? "3-wire bus protocol": 
  	  "invalid");
#endif

  return ret;

#else
  icc->type = ICC_SYNC_MEMORY_TYPE;

  return ICC_SYNC_OK;
#endif
}

static int
ICC_Sync_ProbeMemoryLength (ICC_Sync * icc)
{
  int ret;
#ifndef ICC_SYNC_MEMORY_LENGTH
  unsigned min, max;
  BYTE status[1];

  if (icc->atr != NULL)
    {
      icc->length = ATR_Sync_GetNumberOfDataUnits(icc->atr) * ATR_Sync_GetLengthOfDataUnits(icc->atr) / 8;
      ret = ICC_SYNC_OK;
    }

  else
    {
      if  (icc->type == ICC_SYNC_I2C_SHORT)
        {
	  min = 256L;
	  max = 2048L;
	}
      else if (icc->type == ICC_SYNC_I2C_LONG)
        {
	  min = 2048L;
	  max = 32768L;
	}
      else
	{
	  min = 256L;
	  max = 2048L;
	}

      for (icc->length = min; icc->length<max; icc->length*=2)
        {
          IFD_Towitoko_SetReadAddress (icc->ifd,  icc->type, icc->length);
          IFD_Towitoko_GetStatus  (icc->ifd, status);
          IFD_Towitoko_DeactivateICC (icc->ifd);
          IFD_Towitoko_ActivateICC (icc->ifd);

          if ((status[0] & 0x10) == 0x10)
	    break;
	}

      ret = ICC_SYNC_OK;
    }

#else
  icc->length = ICC_SYNC_MEMORY_LENGTH;
  ret = ICC_SYNC_OK;
#endif

#ifdef DEBUG_ICC
  printf ("ICC: Memory size = %d\n", icc->length);
#endif

  return ret;
}

static int
ICC_Sync_ProbePagemode (ICC_Sync * icc)
{
  if (icc->type == ICC_SYNC_I2C_LONG)
    icc->pagemode = 0x40;
  else
    icc->pagemode = 0x00;

  return ICC_SYNC_OK;
}

static ATR_Sync * 
ICC_Sync_CreateAtr(ICC_Sync * icc)
{
  ATR_Sync * atr;
  BYTE atr_buffer[ATR_SYNC_SIZE], protocol;

  if (icc->atr !=NULL)
    return icc->atr;

  protocol = (icc->type==ICC_SYNC_I2C_SHORT)? ATR_SYNC_PROTOCOL_TYPE_SDA:
	(icc->type==ICC_SYNC_I2C_LONG)? ATR_SYNC_PROTOCOL_TYPE_SDA:
	(icc->type==ICC_SYNC_2W)? ATR_SYNC_PROTOCOL_TYPE_2W:
	(icc->type==ICC_SYNC_3W)? ATR_SYNC_PROTOCOL_TYPE_3W: ATR_SYNC_PROTOCOL_TYPE_SDA;
  
  atr_buffer[0] = (protocol << 4) | ATR_SYNC_STRUCTURE_ID_GENERAL;
  atr_buffer[1] = (icc->length == 256L)? 0x13:
	  (icc->length == 512L)? 0x1B:
	  (icc->length == 1024L)? 0x23:
	  (icc->length == 2048L)? 0x2B:
	  (icc->length == 4096L)? 0x33:
	  (icc->length == 8192L)? 0x3B: 
	  (icc->length == 16384)? 0x43:
	  (icc->length == 32768)? 0x4B: 0x4B;
  atr_buffer[2] = 0x10;
  atr_buffer[3] = 0x84;

  atr = ATR_Sync_New();
  if (atr!=NULL)
    ATR_Sync_Init (atr, atr_buffer, ATR_SYNC_SIZE);
  
  return atr;
}

static void
ICC_Sync_Clear (ICC_Sync * icc)
{
  icc->ifd = NULL;
  icc->atr = NULL;
  icc->type = 0;
  icc->length = 0;
  icc->pagemode = 0x00;
  memset (icc->pin, 0, ICC_SYNC_PIN_SIZE);
  icc->pin_ok = FALSE;
  icc->pin_needed = FALSE;
  icc->active = FALSE;
  icc->baudrate = 0L;
}
