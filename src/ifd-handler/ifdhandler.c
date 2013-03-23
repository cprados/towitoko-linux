/*
    ifdhandler.c

    Mapping if CT-API / CT-BCS interface to the IFD Hanlder 2.0.
    Getting/Setting IFD/Protocol/ICC parameters other than the ATR is not
    supported. IFDH_MAX_READERS simultaneous readers are supported.

    This file is part of the Unix driver for Towitoko smartcard readers
    Copyright (C) 1998 1999 2000 2001 Carlos Prados <cprados@yahoo.com>

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "pcscdefines.h"
#include "ifdhandler.h"
#include <ctapi.h>
#include <ctbcs.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#ifdef DEBUG_IFDH
#include <syslog.h>
#endif

/*
 * Not exported constants definition
 */

/* Maximum number of readers handled */
#define IFDH_MAX_READERS        4

/* Maximum number of slots per reader handled */
#define IFDH_MAX_SLOTS          1

/*
 * Not exported data types definition
 */

typedef struct
{
  DEVICE_CAPABILITIES device_capabilities;
  ICC_STATE icc_state;
  PROTOCOL_OPTIONS protocol_options;
}
IFDH_Context;

/*
 * Not exported variables definition
 */

/* Matrix that stores conext information of all slots and readers */
static IFDH_Context *ifdh_context[IFDH_MAX_READERS][IFDH_MAX_SLOTS] = {
  {NULL},{NULL},{NULL},{NULL},
};

/* Mutexes for all readers */
#ifdef HAVE_PTHREAD_H
static pthread_mutex_t ifdh_context_mutex[IFDH_MAX_READERS] = {
  PTHREAD_MUTEX_INITIALIZER,
  PTHREAD_MUTEX_INITIALIZER,
  PTHREAD_MUTEX_INITIALIZER,
  PTHREAD_MUTEX_INITIALIZER
};
#endif

/*
 * Exported functions definition
 */

RESPONSECODE IFDHCreateChannel (DWORD Lun, DWORD Channel)
{
  char ret;
  unsigned short ctn, pn, slot;
  RESPONSECODE rv;

  ctn = ((unsigned short) (Lun >> 16)) % IFDH_MAX_READERS;
  slot = ((unsigned short) (Lun & 0x0000FFFF)) % IFDH_MAX_SLOTS;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock (&ifdh_context_mutex[ctn]);
#endif

  if (ifdh_context[ctn][slot] == NULL)
    {
      /* Handle USB CHANNELID numbers */
      if (Channel >= 0x200000)
      /* USB ports on this CT-API starts at 0x8000 */
#ifndef CTAPI_WIN32_COM
          pn = (unsigned short) (Channel & 0x1FFFFF) + 0x8000;
#else
          pn = (unsigned short) (Channel & 0x1FFFFF) + 0x8001;
#endif

      /* Conversion of old-style ifd-hanler 1.0 CHANNELID */
#ifndef CTAPI_WIN32_COM
      else if (Channel == 0x0103F8)
        pn = PORT_COM1;
      else if (Channel == 0x0102F8)
        pn = PORT_COM2;
      else if (Channel == 0x0103E8)
        pn = PORT_COM3;
      else if (Channel == 0x0102E8)
        pn = PORT_COM4;
#else
      else if (Channel == 0x0103F8)
        pn = 1;
      else if (Channel == 0x0102F8)
        pn = 2;
      else if (Channel == 0x0103E8)
        pn = 3;
      else if (Channel == 0x0102E8)
        pn = 4;
#endif

      /* IFD Handler 2.0 CHANNELID*/
      else
#ifndef CTAPI_WIN32_COM
        pn = (unsigned short) Channel - 1;
#else
        pn = (unsigned short) Channel;
#endif

      ret = CT_init (ctn, pn);

      if (ret == OK)
        {
          /* Initialize context of the all slots in this reader */
          for (slot = 0; slot < IFDH_MAX_SLOTS; slot++)
            {
              ifdh_context[ctn][slot] =
                (IFDH_Context *) malloc (sizeof (IFDH_Context));

              if (ifdh_context[ctn][slot] != NULL)
                memset (ifdh_context[ctn][slot], 0, sizeof (IFDH_Context));
            }
          rv = IFD_SUCCESS;
        }

      else
        rv = IFD_COMMUNICATION_ERROR;
    }

  else
    {
      /* Assume that IFDHCreateChannel is being called for another
         already initialized slot in this same reader, and return Success */
      rv = IFD_SUCCESS;
    }

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock (&ifdh_context_mutex[ctn]);
#endif

#ifdef DEBUG_IFDH
  syslog (LOG_INFO, "IFDH: IFDHCreateChannel(Lun=0x%X, Channel=0x%X)=%d",Lun,
          Channel, rv);
#endif

  return rv;
}

RESPONSECODE
IFDHCloseChannel (DWORD Lun)
{
  char ret;
  unsigned short ctn, slot;
  RESPONSECODE rv;

  ctn = ((unsigned short) (Lun >> 16)) % IFDH_MAX_READERS;
  slot = ((unsigned short) (Lun & 0x0000FFFF)) % IFDH_MAX_SLOTS;

  ret = CT_close (ctn);

  if (ret == OK)
    {
#ifdef HAVE_PTHREAD_H
      pthread_mutex_lock (&ifdh_context_mutex[ctn]);
#endif

      /* Free context of the all slots in this reader */
      for (slot = 0; slot < IFDH_MAX_SLOTS; slot++)
        {
          if (ifdh_context[ctn][slot] != NULL)
            {
              free (ifdh_context[ctn][slot]);
              ifdh_context[ctn][slot] = NULL;
            }
        }
#ifdef HAVE_PTHREAD_H
      pthread_mutex_unlock (&ifdh_context_mutex[ctn]);
#endif
      rv = IFD_SUCCESS;
    }

  else
    rv = IFD_COMMUNICATION_ERROR;

#ifdef DEBUG_IFDH
  syslog (LOG_INFO, "IFDH: IFDHCloseChannel(Lun=0x%X)=%d", Lun, rv);
#endif

  return rv;
}

RESPONSECODE
IFDHGetCapabilities (DWORD Lun, DWORD Tag, PDWORD Length, PUCHAR Value)
{
  unsigned short ctn, slot;
  RESPONSECODE rv;

  ctn = ((unsigned short) (Lun >> 16)) % IFDH_MAX_READERS;
  slot = ((unsigned short) (Lun & 0x0000FFFF)) % IFDH_MAX_SLOTS;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock (&ifdh_context_mutex[ctn]);
#endif

  switch (Tag)
  {
    case TAG_IFD_ATR:
    (*Length) = ifdh_context[ctn][slot]->icc_state.ATR_Length;
    memcpy (Value, ifdh_context[ctn][slot]->icc_state.ATR, (*Length));
    rv = IFD_SUCCESS;
    break;

    case TAG_IFD_SLOTS_NUMBER:
    (*Length) = 1;
    (*Value) = IFDH_MAX_SLOTS;
    rv = IFD_SUCCESS;
    break;

    case TAG_IFD_SIMULTANEOUS_ACCESS:
    (*Length) = 1;
    (*Value) = IFDH_MAX_READERS;
    rv = IFD_SUCCESS;
    break;

    default:
    (*Length) = 0;
    rv = IFD_ERROR_TAG;
  }

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock (&ifdh_context_mutex[ctn]);
#endif

#ifdef DEBUG_IFDH
  syslog (LOG_INFO, "IFDH: IFDHGetCapabilities (Lun=0x%X, Tag=0x%X)=%d",Lun, Tag, rv);
#endif

  return rv;
}

RESPONSECODE
IFDHSetCapabilities (DWORD Lun, DWORD Tag, DWORD Length, PUCHAR Value)
{
#ifdef DEBUG_IFDH
/*  syslog (LOG_INFO, "IFDH: IFDHSetCapabilities (Lun=%X, Tag=%X)=%d",Lun, Tag,
          IFD_NOT_SUPPORTED ); */
#endif

  return IFD_NOT_SUPPORTED;
}

RESPONSECODE
IFDHSetProtocolParameters (DWORD Lun, DWORD Protocol,
                           UCHAR Flags, UCHAR PTS1, UCHAR PTS2, UCHAR PTS3)
{
  char ret;
  unsigned short ctn, slot, lc, lr;
  UCHAR cmd[10], rsp[256], sad, dad;
  RESPONSECODE rv;

  ctn = ((unsigned short) (Lun >> 16)) % IFDH_MAX_READERS;
  slot = ((unsigned short) (Lun & 0x0000FFFF)) % IFDH_MAX_SLOTS;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock (&ifdh_context_mutex[ctn]);
#endif

  if (ifdh_context[ctn][slot] != NULL)
    {
      cmd[0] = CTBCS_CLA;
      cmd[1] = CTBCS_INS_RESET;
      cmd[2] = (UCHAR) (slot + 1);
      cmd[3] = CTBCS_P2_RESET_GET_ATR;
      cmd[4] = 0x06;
      cmd[5] = 0xFF;
      cmd[6] = (Flags << 4) | (0x0F & Protocol);

      lc=7; 

      if ((Flags & 0x10) == 0x10)
	  cmd[lc++] = PTS1;
      
      if ((Flags & 0x20) == 0x20)
	  cmd[lc++] = PTS2;

      if ((Flags & 0x40) == 0x40)
	  cmd[lc++] = PTS3;

      dad = 0x01;
      sad = 0x02;
      lr = 256;

      ret = CT_data (ctn, &dad, &sad, lc, cmd, &lr, rsp);

      if ((ret == OK) && (lr >= 2))
        {
          ifdh_context[ctn][slot]->icc_state.ATR_Length = (DWORD) lr - 2;
          memcpy (ifdh_context[ctn][slot]->icc_state.ATR, rsp, lr - 2);

          rv = IFD_SUCCESS;
        }

      else
        rv = IFD_ERROR_PTS_FAILURE;
    }

  else
    rv = IFD_ICC_NOT_PRESENT;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock (&ifdh_context_mutex[ctn]);
#endif

#ifdef DEBUG_IFDH
  syslog (LOG_INFO, "IFDH: IFDHSetProtocolParameters (Lun=0x%X, Protocol=%d, Flags=0x%02X, PTS1=0x%02X, PTS2=0x%02X, PTS3=0x%02X)=%d", 
	  Lun, Protocol, Flags, PTS1, PTS2, PTS3, rv);
#endif

  return rv;
}

RESPONSECODE
IFDHPowerICC (DWORD Lun, DWORD Action, PUCHAR Atr, PDWORD AtrLength)
{
  char ret;
  unsigned short ctn, slot, lc, lr;
  UCHAR cmd[5], rsp[256], sad, dad;
  RESPONSECODE rv;

  ctn = ((unsigned short) (Lun >> 16)) % IFDH_MAX_READERS;
  slot = ((unsigned short) (Lun & 0x0000FFFF)) % IFDH_MAX_SLOTS;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock (&ifdh_context_mutex[ctn]);
#endif

  if (ifdh_context[ctn][slot] != NULL)
    {
      if (Action == IFD_POWER_UP)
        {
          cmd[0] = CTBCS_CLA;
          cmd[1] = CTBCS_INS_REQUEST;
          cmd[2] = (UCHAR) (slot + 1);
          cmd[3] = CTBCS_P2_REQUEST_GET_ATR;
          cmd[4] = 0x00;

          dad = 0x01;
          sad = 0x02;
          lr = 256;
          lc = 5;

          ret = CT_data (ctn, &dad, &sad, 5, cmd, &lr, rsp);

          if ((ret == OK) && (lr >= 2))
            {
              ifdh_context[ctn][slot]->icc_state.ATR_Length = (DWORD) lr - 2;
              memcpy (ifdh_context[ctn][slot]->icc_state.ATR, rsp, lr - 2);

              (*AtrLength) = (DWORD) lr - 2;
              memcpy (Atr, rsp, lr - 2);

              rv = IFD_SUCCESS;
            }

          else
            rv = IFD_COMMUNICATION_ERROR;
        }

      else if (Action == IFD_POWER_DOWN)
        {
          cmd[0] = CTBCS_CLA;
          cmd[1] = CTBCS_INS_EJECT;
          cmd[2] = (UCHAR) (slot + 1);
          cmd[3] = 0x00;
          cmd[4] = 0x00;

          dad = 0x01;
          sad = 0x02;
          lr = 256;
          lc = 5;

          ret = CT_data (ctn, &dad, &sad, 5, cmd, &lr, rsp);

          if (ret == OK)
            {
              ifdh_context[ctn][slot]->icc_state.ATR_Length = 0;
              memset (ifdh_context[ctn][slot]->icc_state.ATR, 0, MAX_ATR_SIZE);

              (*AtrLength) = 0;
              rv = IFD_SUCCESS;
            }

          else
            rv = IFD_COMMUNICATION_ERROR;
        }

      else if (Action == IFD_RESET)
        {
          cmd[0] = CTBCS_CLA;
          cmd[1] = CTBCS_INS_RESET;
          cmd[2] = (UCHAR) (slot + 1);
          cmd[3] = CTBCS_P2_RESET_GET_ATR;
          cmd[4] = 0x00;

          dad = 0x01;
          sad = 0x02;
          lr = 256;
          lc = 5;

          ret = CT_data (ctn, &dad, &sad, 5, cmd, &lr, rsp);

          if ((ret == OK) && (lr >= 2))
            {
              ifdh_context[ctn][slot]->icc_state.ATR_Length = (DWORD) lr - 2;
              memcpy (ifdh_context[ctn][slot]->icc_state.ATR, rsp, lr - 2);

              (*AtrLength) = (DWORD) lr - 2;
              memcpy (Atr, rsp, lr - 2);

              rv = IFD_SUCCESS;
            }

          else
            rv = IFD_ERROR_POWER_ACTION;
        }

      else
        rv = IFD_NOT_SUPPORTED;
    }

  else
    rv = IFD_ICC_NOT_PRESENT;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock (&ifdh_context_mutex[ctn]);
#endif

#ifdef DEBUG_IFDH
  syslog (LOG_INFO, "IFDH: IFDHPowerICC (Lun=0x%X, Action=0x%X)=%d", Lun, Action, rv);
#endif

  return rv;
}

RESPONSECODE
IFDHTransmitToICC (DWORD Lun, SCARD_IO_HEADER SendPci,
                   PUCHAR TxBuffer, DWORD TxLength,
                   PUCHAR RxBuffer, PDWORD RxLength, PSCARD_IO_HEADER RecvPci)
{
  char ret;
  unsigned short ctn, slot, lc, lr;
  UCHAR sad, dad;
  RESPONSECODE rv;

  ctn = ((unsigned short) (Lun >> 16)) % IFDH_MAX_READERS;
  slot = ((unsigned short) (Lun & 0x0000FFFF)) % IFDH_MAX_SLOTS;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock (&ifdh_context_mutex[ctn]);
#endif

  if (ifdh_context[ctn][slot] != NULL)
    {
#ifdef HAVE_PTHREAD_H
      pthread_mutex_unlock (&ifdh_context_mutex[ctn]);
#endif
      dad = (UCHAR) ((slot == 0) ? 0x00 : slot + 1);
      sad = 0x02;
      lr = (unsigned short) (*RxLength);
      lc = (unsigned short) TxLength;

      ret = CT_data (ctn, &dad, &sad, lc, TxBuffer, &lr, RxBuffer);

      if (ret == OK)
        {
          (*RxLength) = lr;
          rv = IFD_SUCCESS;
        }

      else
        {
          (*RxLength) = 0;
          rv = IFD_COMMUNICATION_ERROR;
        }
    }

  else
    {
#ifdef HAVE_PTHREAD_H
      pthread_mutex_unlock (&ifdh_context_mutex[ctn]);
#endif
      rv = IFD_ICC_NOT_PRESENT;
    }

#ifdef DEBUG_IFDH
  syslog (LOG_INFO, "IFDH: IFDHTransmitToICC (Lun=0x%X, Tx=%u, Rx=%u)=%d", Lun, TxLength, (*RxLength), rv);
#endif

  return rv;
}

RESPONSECODE
IFDHControl (DWORD Lun, PUCHAR TxBuffer,
             DWORD TxLength, PUCHAR RxBuffer, PDWORD RxLength)
{
  char ret;
  unsigned short ctn, slot, lc, lr;
  UCHAR sad, dad;
  RESPONSECODE rv;

  ctn = ((unsigned short) (Lun >> 16)) % IFDH_MAX_READERS;
  slot = ((unsigned short) (Lun & 0x0000FFFF)) % IFDH_MAX_SLOTS;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock (&ifdh_context_mutex[ctn]);
#endif

  if (ifdh_context[ctn][slot] != NULL)
    {
#ifdef HAVE_PTHREAD_H
      pthread_mutex_unlock (&ifdh_context_mutex[ctn]);
#endif
      dad = 0x01;
      sad = 0x02;
      lr = (unsigned short) (*RxLength);
      lc = (unsigned short) TxLength;

      ret = CT_data (ctn, &dad, &sad, lc, TxBuffer, &lr, RxBuffer);

      if (ret == OK)
        {
          (*RxLength) = lr;
          rv = IFD_SUCCESS;
        }
      else
        {
          (*RxLength) = 0;
          rv = IFD_COMMUNICATION_ERROR;
        }
    }

  else
    {
#ifdef HAVE_PTHREAD_H
      pthread_mutex_unlock (&ifdh_context_mutex[ctn]);
#endif
      rv = IFD_ICC_NOT_PRESENT;
    }

#ifdef DEBUG_IFDH
  syslog (LOG_INFO, "IFDH: IFDHControl (Lun=0x%X, Tx=%u, Rx=%u)=%d", Lun, TxLength, (*RxLength), rv);
#endif

  return rv;
}

RESPONSECODE
IFDHICCPresence (DWORD Lun)
{
  char ret;
  unsigned short ctn, slot, lc, lr;
  UCHAR cmd[5], rsp[256], sad, dad;
  RESPONSECODE rv;

  ctn = ((unsigned short) (Lun >> 16)) % IFDH_MAX_READERS;
  slot = ((unsigned short) (Lun & 0x0000FFFF)) % IFDH_MAX_SLOTS;

  cmd[0] = CTBCS_CLA;
  cmd[1] = CTBCS_INS_STATUS;
  cmd[2] = CTBCS_P1_CT_KERNEL;
  cmd[3] = CTBCS_P2_STATUS_ICC;
  cmd[4] = 0x00;

  dad = 0x01;
  sad = 0x02;
  lc = 5;
  lr = 256;

  ret = CT_data (ctn, &dad, &sad, lc, cmd, &lr, rsp);

  if (ret == OK)
    {
      if (slot < lr - 2)
        {
          if (rsp[slot] == CTBCS_DATA_STATUS_NOCARD)
            rv = IFD_ICC_NOT_PRESENT;
          else
            rv = IFD_ICC_PRESENT;
        }
      else
        rv = IFD_ICC_NOT_PRESENT;
    }
  else
    rv = IFD_COMMUNICATION_ERROR;

#ifdef DEBUG_IFDH
  syslog (LOG_INFO, "IFDH: IFDHICCPresence (Lun=0x%X)=%d", Lun, rv);
#endif

  return rv;
}
