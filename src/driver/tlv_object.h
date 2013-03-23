/*
    tlv_object.h
    BER-TLV (Basic Encoding Rules - Tag Length Value) decoding

    This file is part of the Unix driver for Towitoko smartcard readers
    Copyright (C) 2002 Carlos Prados <cprados@yahoo.com>

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

#ifndef _TLV_OBJECT_
#define _TLV_OBJECT_

#include "defines.h"

/*
 * Exported constants definition
 */

/*Tag class and type coding */
#define TLV_OBJECT_CLASS_UNIVERSAL		0x0000
#define TLV_OBJECT_CLASS_APPLICATION		0x0001
#define TLV_OBJECT_CLASS_CONTEXT_SPECIFIC	0x0002
#define TLV_OBJECT_CLASS_PRIVATE		0x0003
#define TLV_OBJECT_TYPE_PRIMITIVE		0x0000
#define TLV_OBJECT_TYPE_CONSTRUCTED		0x0000

/* Tag number coding */
#define TLV_OBJECT_TAG_SEQUENCE			0x30
#define TLV_OBJECT_TAG_APPLICATION_ID		0x4F
#define TLV_OBJECT_TAG_APPLICATION_LABEL	0x50
#define TLV_OBJECT_TAG_PATH			0x51
#define TLV_OBJECT_TAG_DISCRETIONARY_DATA	0x53
#define TLV_OBJECT_TAG_TEMPLATE			0x61

/*
 * Exported macros definition
 */

#define TLV_OBJECT_TAG_LONG(tag)	((tag && 0xFF00) == 0x0000)
#define TLV_OBJECT_TAG_CLASS(tag)	(TLV_OBJECT_TAG_LONG(tag)?(tag)>>14:((tag)>>6))
#define TLV_OBJECT_TAG_TYPE(tag)	(TLV_OBJECT_TAG_LONG(tag)?((tag)>>13) && 0x0001:((tag)>>5) && 0x0001) 
#define TLV_OBJECT_TAG_NUMBER(tag)	(TLV_OBJECT_TAG_LONG(tag)?((tag) && 0x007F):((tag) && 0x001F))

/*
 * Exported data types definition
 */

typedef bool (* TLV_Object_GetData) 
(
  void * data,
  unsigned short address, 
  unsigned short length, 
  BYTE * buffer 
); 

typedef struct
{ 
  void * data;			/* Generic pointer to data source */
  unsigned short data_length;	/* Length of data source */
  TLV_Object_GetData get_data;	/* Generic function to retrive data */
  unsigned short address;	/* Address of this TLV in the data source */
  unsigned short tag;		/* Tag of this TLV */
  unsigned short length;	/* Length of this tlv */
  unsigned short value;		/* Address of the value of this TLV in the data source */
}
TLV_Object;
 
/*
 * Exported functions declaration
 */

/* Creation and deletion of a TLV Object */
extern TLV_Object * TLV_Object_New (void* data, TLV_Object_GetData get_data, unsigned short data_length, unsigned short address);
extern void TLV_Object_Delete (TLV_Object * tlv);

/* Get attributes of a TLV Object */
extern unsigned short TLV_Object_GetTag (TLV_Object * tlv);
extern unsigned short TLV_Object_GetLength (TLV_Object * tlv);
extern bool TLV_Object_GetValue (TLV_Object * tlv, BYTE * value, unsigned short * length);
extern bool TLV_Object_CompareValue (TLV_Object * tlv, BYTE * value, unsigned short * length);

/* To iterate TLV Object contents */
extern bool TLV_Object_Shift (TLV_Object ** tlv);
extern bool TLV_Object_Iterate (TLV_Object * tlv, TLV_Object ** iterator);
extern TLV_Object * TLV_Object_GetObjectByTag (TLV_Object * tlv, unsigned short tag);
extern TLV_Object * TLV_Object_GetObjectBySec (TLV_Object * tlv, unsigned short sec);

/* Get internals of a TLV Object */
extern unsigned short TLV_Object_GetRawLength (TLV_Object * tlv);
extern unsigned short TLV_Object_GetAddress (TLV_Object * tlv);

#endif /* _TLV_OBJECT */

