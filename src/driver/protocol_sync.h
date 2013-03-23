/*
    protocol_sync.h
    MCT v0.9 part 7 interindustry command set for synchronous cards 

    This file is part of the Unix driver for Towitoko smartcard readers
    Copyright (C) 2000, 2001, 2002 Carlos Prados <cprados@yahoo.com>

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

#ifndef _PROTOCOL_SYNC_
#define _PROTOCOL_SYNC_

#include "defines.h"
#include "icc_sync.h"
#include "apdu.h"

/*
 * Exported constants definition
 */

/* Return codes */
#define PROTOCOL_SYNC_OK		0	/* Command OK */
#define PROTOCOL_SYNC_ICC_ERROR		1	/* ICC comunication error */
#define PROTOCOL_SYNC_ERROR		2	/* Protocol Error */

/*
 * Exported datatypes definition
 */

/* Protocol handler */
typedef struct
{
  ICC_Sync *icc;		/* Synchrosous integrated cirtuit card */
  unsigned path;		/* start byte of selected data section */
  unsigned length;		/* length of selected data section */
}
Protocol_Sync;

/*
 * Exported functions declaration
 */

/* Create a new protocol handler */
extern Protocol_Sync *Protocol_Sync_New (void);

/* Delete a protocol handler */
extern void Protocol_Sync_Delete (Protocol_Sync * ps);

/* Inicialice a protocol handler */
extern int Protocol_Sync_Init (Protocol_Sync * ps, ICC_Sync * icc);

/* Send a command and return a response */
extern int Protocol_Sync_Command (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp);

/* Delete a protocol hanlder */
extern int Protocol_Sync_Close (Protocol_Sync * ps);

#endif /* _PROTOCOL_SYNC_ */

