/*
    icc_sync.h
    Synchronous integrated circuit cards handling functions

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

#ifndef _ICC_SYNC_
#define _ICC_SYNC_

#include "defines.h"
#include "ifd_towitoko.h"
#include "atr_sync.h"

/*
 * Exported constants definition
 */

/* Return codes */
#define ICC_SYNC_OK		0
#define ICC_SYNC_DETECT_ERROR	1
#define ICC_SYNC_IFD_ERROR	2
#define ICC_SYNC_RO_ERROR	3
#define ICC_SYNC_PIN_ERROR	4
#define ICC_SYNC_BLOCKED_ERROR	5

/* ICC types */
#define ICC_SYNC_I2C_SHORT	0
#define ICC_SYNC_I2C_LONG	1
#define ICC_SYNC_2W		2
#define ICC_SYNC_3W		3

/* Maximum size of the PIN */
#define ICC_SYNC_PIN_SIZE	3

/*
 * Exported types definition
 */

/* Synchronous ICC */
typedef struct
{
  IFD *ifd;			/* Interface device */
  ATR_Sync *atr;		/* Answer to reset */
  int type;			/* Synchronous Card type */
  unsigned length;		/* Memory length */
  BYTE pagemode;		/* I2C pagemode */
  BYTE pin[ICC_SYNC_PIN_SIZE];	/* 2W and 3W pin */
  bool pin_ok;			/* pin is correct */
  bool pin_needed;		/* pin has to be entered */
  bool active;			/* ICC is active */
  unsigned long baudrate;       /* Current baudrate (bps) for transmiting to this ICC */
}
ICC_Sync;

/*
 * Exported functions declaration
 */

/* ICC_Sync creation and deletion */
extern ICC_Sync *ICC_Sync_New (void);
extern void ICC_Sync_Delete (ICC_Sync * icc);

/* Card initialization and deactivation */
int ICC_Sync_Init (ICC_Sync * icc, IFD * ifd);
int ICC_Sync_Close (ICC_Sync * icc);

/* Get ICC atributes */
unsigned ICC_Sync_GetLength (ICC_Sync * icc);
int ICC_Sync_GetType (ICC_Sync * icc);
BYTE ICC_Sync_GetPagemode (ICC_Sync * icc);
IFD *ICC_Sync_GetIFD (ICC_Sync * icc);
ATR_Sync *ICC_Sync_GetAtr (ICC_Sync * icc);
int ICC_Sync_SetBaudrate (ICC_Sync * icc, unsigned long baudrate);
int ICC_Sync_GetBaudrate (ICC_Sync * icc, unsigned long * baudrate);

/* ICC operations */
int ICC_Sync_BeginTransmission (ICC_Sync * icc);
int ICC_Sync_Read (ICC_Sync * icc, unsigned short address, unsigned length, BYTE * data);
int ICC_Sync_Write (ICC_Sync * icc, unsigned short address, unsigned length, BYTE * data);
int ICC_Sync_EnterPin (ICC_Sync * icc, BYTE * pin, unsigned *trials);
int ICC_Sync_ChangePin (ICC_Sync * icc, BYTE * pin);

#endif /* _ICC_SYNC_ */

