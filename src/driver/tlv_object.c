/*
    tlv_object.c
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

#include <stdio.h>

#include <stdlib.h>
#include "defines.h"
#include "tlv_object.h"

/*
 * Exported functions definition
 */

TLV_Object * 
TLV_Object_New (void* data, TLV_Object_GetData get_data, unsigned short data_length, unsigned short address)
{
  TLV_Object * tlv;
  BYTE buffer[2];
  unsigned i;

  tlv = (TLV_Object *) malloc (sizeof(TLV_Object));

  if (tlv != NULL)
    {
      tlv->data = data;
      tlv->get_data = get_data;
      tlv->data_length = data_length;
      tlv->address = address;

      /* Get Tag */
      i=0;

      if ((address >= data_length) || !((*get_data)(data, address, 1, buffer)))
        {
          free (tlv);
	  return NULL;
	}

      /* Two bytes tag */
      if ((buffer[0] & 0x1F) == 0x1F) 
        {
	  i++;

	  if ((address+i >= data_length) || !((*get_data)(data, address+i, 1, buffer+i)))
	    {
	      free (tlv);
	      return  NULL;
	    }

	  tlv->tag = (buffer[0] << 8) | buffer[1];
	}
      
      /* One byte tag */
      else
        tlv->tag = 0x00FF & buffer[0];

      /* Get length */
      i++;
      
      if ((address+i >= data_length) || !((*get_data)(data, address+i, 1, buffer)))
        {
	  free (tlv);
	  return NULL;
        }

      /* Length > 127 */
      if ((buffer[0] & 0x80) == 0x80)
        {
	  i++;

	  /* 256 > length > 127 */
	  if ((buffer[0] & 0x7F) == 0x01)
	    {
	      if ((address+i >= data_length) || !((*get_data)(data, address+i, 1, buffer)))
	        {
		  free (tlv);
		  return NULL;
		}

	      tlv->length = 0x00FF & buffer[0];
	      i++;
	    }

	  /* 65536 > length > 255 */
	  else if ((buffer[0] & 0x7F) == 0x02)
	    {
	      if ((address+i+1 >= data_length) || !((*get_data)(data, address+i, 2, buffer)))
	        {
		  free (tlv);
		  return NULL;
		}

	      tlv->length = (buffer[0] << 8) | buffer[1];
	      i+=2;
	    }

	 else
	    {
	      free (tlv);
	      return NULL;
	    }
	}

      /* Length < 128 */
      else
        {
	  i++;
	  tlv->length = 0x7F & buffer[0];
	}

      /* Get value address */
      if (address+i >= data_length)
        {
	  free (tlv);
	  return NULL;
	}

      tlv->value = address + i;

      /* Round tlv object length to fit within available data */
      tlv->length = MIN (tlv->length, data_length - tlv->value);
    }

  return tlv;
}

void 
TLV_Object_Delete (TLV_Object * tlv)
{
  free (tlv);
}

unsigned short 
TLV_Object_GetTag (TLV_Object * tlv)
{
  return tlv->tag;
}

unsigned short 
TLV_Object_GetLength (TLV_Object * tlv)
{
  return tlv->length;
}

bool
TLV_Object_GetValue (TLV_Object * tlv, BYTE * value, unsigned short *length)
{
  (*length) = MIN ((*length), tlv->length);

  if ((*length) == 0)
    return TRUE;

  return ((*(tlv->get_data)) (tlv->data, tlv->value, (*length), value));
}

bool
TLV_Object_CompareValue (TLV_Object * tlv, BYTE * value, unsigned short *length)
{
  BYTE * buffer;
  bool ret;

  buffer = (BYTE *) calloc ((*length), sizeof (BYTE));

  if (TLV_Object_GetValue (tlv, buffer, length))
    ret = !(memcmp (value, buffer, (*length)));
  else
    ret = FALSE;

  return ret;
}

bool
TLV_Object_Shift (TLV_Object ** tlv)
{
  TLV_Object * aux;
  
  aux = TLV_Object_New ((*tlv)->data, (*tlv)->get_data, (*tlv)->data_length , (*tlv)->value + (*tlv)->length);
  TLV_Object_Delete (*tlv);

  (*tlv) = aux;
  return (aux != NULL);
}

bool
TLV_Object_Iterate (TLV_Object * tlv, TLV_Object ** iterator)
{
  TLV_Object * next;

  if ((*iterator) == NULL)
    next = TLV_Object_New (tlv->data, tlv->get_data, tlv->data_length, tlv->value);

  else
    {
      next = (*iterator);

      if (next->value + next->length < tlv->value + tlv->length)
        TLV_Object_Shift (&next);

      else
        {
          TLV_Object_Delete (next);
          next = NULL;
        }
    }

  return (((*iterator) = next) != NULL);
}

TLV_Object * 
TLV_Object_GetObjectByTag (TLV_Object * tlv, unsigned short tag)
{
  TLV_Object * aux;

  aux = TLV_Object_New (tlv->data, tlv->get_data, tlv->data_length, tlv->value);

  while ((aux) && (TLV_Object_GetTag (aux) != tag)) 
    {
      if (aux->value + aux->length < tlv->value + tlv->length)
        TLV_Object_Shift (&aux);

      else
        {
	  TLV_Object_Delete (aux);
	  aux = NULL;
	}
    }
  
  return aux;
}

TLV_Object * 
TLV_Object_GetObjectBySec (TLV_Object * tlv, unsigned short sec)
{
  TLV_Object * aux;
  unsigned short i;

  aux = TLV_Object_New (tlv->data, tlv->get_data, tlv->data_length, tlv->value);
  
  for (i=0; ((i<sec) && (aux)); i++)
    {
      if (aux->value + aux->length < tlv->value + tlv->length)
        TLV_Object_Shift(&aux);

      else
        {
	  TLV_Object_Delete(aux);
	  aux = NULL;
	}
    }

  return aux;
}

unsigned short 
TLV_Object_GetRawLength (TLV_Object * tlv)
{
  return (tlv->length + (tlv->value - tlv->address));
}

unsigned short
TLV_Object_GetAddress (TLV_Object * tlv)
{
  return tlv->address;
}

