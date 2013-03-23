/*
    atr_sync.c
    Memory cards answer to reset abstract data type

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

#include "atr_sync.h"
#include <stdlib.h>

/* 
 * Exported functions definition
 */

ATR_Sync *
ATR_Sync_New (void)
{
  ATR_Sync *atr;

  atr = (ATR_Sync *) malloc (sizeof (ATR_Sync));

  if (atr != NULL)
    {
      atr->H1 = 0x00;
      atr->H2 = 0x00;
      atr->H3 = 0x00;
      atr->H4 = 0x00;
    }

  return atr;
}

int
ATR_Sync_Init (ATR_Sync * atr, BYTE * buffer, unsigned length)
{
  int ret;

  if (length < 4)
    return ATR_SYNC_MALFORMED;

  atr->H1 = buffer[0];
  atr->H2 = buffer[1];
  atr->H3 = buffer[2];
  atr->H4 = buffer[3];

  if ((buffer[0] & 0x03) == 0x02)
    {
      if (buffer[2] == 0x10)
	ret = ATR_SYNC_OK;
      else
	ret = ATR_SYNC_MALFORMED;
    }
  else
    ret = ATR_SYNC_UNKNOWN_STRUCTURE;

  return ret;
}

BYTE
ATR_Sync_GetProtocolType (ATR_Sync * atr)
{
  return (((atr->H1) >> 4) & 0x0F);
}

BYTE
ATR_Sync_GetStructureID (ATR_Sync * atr)
{
  return ((atr->H1) & 0x07);
}

BYTE
ATR_Sync_GetProtocolParameter (ATR_Sync * atr)
{
  return (atr->H2);
}

unsigned
ATR_Sync_GetNumberOfDataUnits (ATR_Sync * atr)
{
  unsigned exp, ret, i;

  exp = ((atr->H2) >> 3) & 0x0F;

  if (exp == 0)
    return 0;

  /* Calculate ret = 2 ^ (exp +6) */
  ret = 64;

  for (i = 0; i < exp; i++)
    ret *= 2;

  return ret;
}

unsigned
ATR_Sync_GetLengthOfDataUnits (ATR_Sync * atr)
{
  unsigned exp, ret, i;

  exp = ((atr->H2) & 0x07);

  /* Calculate ret = (2 ^ exp) */
  ret = 1;

  for (i = 0; i < exp; i++)
    ret *= 2;

  return ret;
}

bool
ATR_Sync_GetReadToEnd (ATR_Sync * atr)
{
  return ((atr->H2 & 0x80) == 0x00);
}

void
ATR_Sync_GetHistoricalBytes (ATR_Sync * atr, BYTE * buffer, unsigned *length)
{
  buffer[0] = atr->H3;
  buffer[1] = atr->H4;
  (*length) = 2;
}

BYTE
ATR_Sync_GetCategoryIndicator (ATR_Sync * atr)
{
  return (atr->H3);
}

BYTE
ATR_Sync_GetDirDataReference (ATR_Sync * atr)
{
  return (atr->H4);
}

void
ATR_Sync_GetRaw (ATR_Sync * atr, BYTE * buffer, unsigned *length)
{
  buffer[0] = atr->H1;
  buffer[1] = atr->H2;
  buffer[2] = atr->H3;
  buffer[3] = atr->H4;
  (*length) = 4;
}

void
ATR_Sync_Delete (ATR_Sync * atr)
{
  free (atr);
}
