/*
    atr_sync.h
    Memory cards answer to reset abstract data type definitions

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

#ifndef _ATR_SYNC_
#define _ATR_SYNC_

#include "defines.h"

/*
 * Exported constants definitions
 */

/* Length in bytes of ATR */
#define ATR_SYNC_SIZE			4
#define ATR_SYNC_HISTORICAL_SIZE	2

/* Return codes */
#define ATR_SYNC_OK			0
#define ATR_SYNC_UNKNOWN_STRUCTURE	1
#define ATR_SYNC_MALFORMED		2

/* Protocol types */
#define ATR_SYNC_IS_PROTOCOL_TYPE_ISO(type)	(((type) | 0xF7) == 0xF7)
#define ATR_SYNC_IS_PROTOCOL_TYPE_OTHER(type)	((((type) & 0x08) == 0x08) && \
						((type) != 0xFF))

#define ATR_SYNC_PROTOCOL_TYPE_SDA		0x08
#define ATR_SYNC_PROTOCOL_TYPE_3W		0x09
#define ATR_SYNC_PROTOCOL_TYPE_2W		0x0A
#define ATR_SYNC_PROTOCOL_TYPE_RFU		0x0F

/* Structure Identifiers */
#define ATR_SYNC_IS_STRUCTURE_ID_ISO(sid)	(((sid) & 0x03) == 0x00)
#define ATR_SYNC_STRUCTURE_ID_GENERAL		0x02
#define ATR_SYNC_STRUCTURE_ID_PROPIETARY	0x06
#define ATR_SYNC_IS_STRUCTURE_ID_SPECIAL(sid)	((((sid) & 0x03) == 0x01) || \
						(((sid) & 0x03) == 0x011))
/* Category Indicators */
#define ATR_SYNC_CATEGORY_INDICATOR		0x10

/* DIR data reference */
#define ATR_SYNC_IS_DIR_DATA_REFERENCE(ddr)	(((ddr) & 0x80) == 0x80)
#define ATR_SYNC_DIR_DATA_REFERENCE(ddr)	((ddr) & 0x7F)

/*
 * Exported data types definition
 */

/* Answer to reset for Synchronous ICC's */
typedef struct
{
  BYTE H1;			/* Protocol type */
  BYTE H2;			/* Protocol parameter */
  BYTE H3;			/* Category Indicator */
  BYTE H4;			/* DIR data reference */
}
ATR_Sync;

/* 
 * Exported functions declaration
 */

extern ATR_Sync *ATR_Sync_New (void);

extern int ATR_Sync_Init (ATR_Sync * atr, BYTE * buffer, unsigned length);

extern BYTE ATR_Sync_GetProtocolType (ATR_Sync * atr);

extern BYTE ATR_Sync_GetStructureID (ATR_Sync * atr);

extern BYTE ATR_Sync_GetProtocolParameter (ATR_Sync * atr);

extern unsigned ATR_Sync_GetNumberOfDataUnits (ATR_Sync * atr);

extern unsigned ATR_Sync_GetLengthOfDataUnits (ATR_Sync * atr);

extern bool ATR_Sync_GetReadToEnd (ATR_Sync * atr);

extern void ATR_Sync_GetHistoricalBytes (ATR_Sync * atr, BYTE * buffer, unsigned *length);

extern BYTE ATR_Sync_GetCategoryIndicator (ATR_Sync * atr);

extern BYTE ATR_Sync_GetDirDataReference (ATR_Sync * atr);

extern void ATR_Sync_GetRaw (ATR_Sync * atr, BYTE * buffer, unsigned *length);

extern void ATR_Sync_Delete (ATR_Sync * atr);

#endif /* _ATR_SYNC_ */

