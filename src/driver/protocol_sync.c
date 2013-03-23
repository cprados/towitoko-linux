/*
    protocol_sync.c
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

#include <stdio.h>

#include "defines.h"
#include "protocol_sync.h"
#include "tlv_object.h"
#include <string.h>
#include <stdlib.h>

/*
 * Not exported constants definition
 */

/* Selectable Data Section Files */
#define PROTOCOL_SYNC_DS_MF	0x3F00
#define PROTOCOL_SYNC_DS_DIR	0x2F00
#define PROTOCOL_SYNC_DS_ATR	0x2F01

/* Max size of application ID */
#define PROTOCOL_SYNC_AID_SIZE	16

/*
 * Not exported macros definition
 */

#define PROTOCOL_SYNC_TLV(icc, address) (TLV_Object_New ((icc), \
					Protocol_Sync_GetData, \
					ICC_Sync_GetLength (icc), \
					(address)))

/*
 * Not exported functions declaration
 */

static int Protocol_Sync_SelectFile (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp);
static int Protocol_Sync_ReadBinary (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp);
static int Protocol_Sync_UpdateBinary (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp);
static int Protocol_Sync_Verify (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp);
static int Protocol_Sync_ChangeVerifyData (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp);
static int Protocol_Sync_BadCommand (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp);
static bool Protocol_Sync_GetData (void * data, unsigned short address, unsigned short length, BYTE * buffer);
static void Protocol_Sync_Clear (Protocol_Sync * ps);

/*
 * Exported functions definition
 */

Protocol_Sync *
Protocol_Sync_New (void)
{
  Protocol_Sync *ps;

  ps = (Protocol_Sync *) malloc (sizeof (Protocol_Sync));

  if (ps != NULL)
    Protocol_Sync_Clear (ps);

  return ps;
}

int
Protocol_Sync_Init (Protocol_Sync * ps, ICC_Sync * icc)
{
  ps->icc = icc;

  /* Select Master File by default */
  ps->path = 0;
  ps->length = ICC_Sync_GetLength (ps->icc);

  return PROTOCOL_SYNC_OK;
}

int
Protocol_Sync_Command (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp)
{
  int ret;

  ICC_Sync_BeginTransmission (ps->icc);

  /* Interindustry Commands */
  switch (APDU_Cmd_Ins (cmd))
    {
    case 0xA4:
      ret = Protocol_Sync_SelectFile (ps, cmd, rsp);
      break;
    case 0xB0:
      ret = Protocol_Sync_ReadBinary (ps, cmd, rsp);
      break;
    case 0xD6:
      ret = Protocol_Sync_UpdateBinary (ps, cmd, rsp);
      break;
    case 0x20:
      ret = Protocol_Sync_Verify (ps, cmd, rsp);
      break;
    case 0x24:
      ret = Protocol_Sync_ChangeVerifyData (ps, cmd, rsp);
      break;
    default:
      ret = Protocol_Sync_BadCommand (ps, cmd, rsp);
    }

  return ret;
}

int
Protocol_Sync_Close (Protocol_Sync * ps)
{
  Protocol_Sync_Clear (ps);

  return PROTOCOL_SYNC_OK;
}

void
Protocol_Sync_Delete (Protocol_Sync * ps)
{
  free (ps);
}

/* 
 * Not exported functions definition 
 */

static int
Protocol_Sync_SelectFile (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp)
{
  BYTE buffer[2], aid[PROTOCOL_SYNC_AID_SIZE], path[2];
  unsigned short fid, aid_length, path_length;
  TLV_Object *tlv_dir, *tlv_template, *tlv_aid, *tlv_path, *tlv_app;
  ATR_Sync *atr;

  /* FID selected */
  if (APDU_Cmd_P1 (cmd) == 0x00)
    {
      /* Get FID */
      fid = (APDU_Cmd_Data (cmd)[0] << 8) | (APDU_Cmd_Data (cmd)[1]);

      if (fid == PROTOCOL_SYNC_DS_MF)
	{
	  ps->path = 0;
	  ps->length = ICC_Sync_GetLength (ps->icc);

	  buffer[0] = 0x90;
	  buffer[1] = 0x00;
	}

      else if (fid == PROTOCOL_SYNC_DS_ATR)
	{
	  atr = ICC_Sync_GetAtr (ps->icc);

	  if ((ATR_Sync_GetCategoryIndicator (atr) == ATR_SYNC_CATEGORY_INDICATOR) &&
	      (ATR_SYNC_IS_DIR_DATA_REFERENCE (ATR_Sync_GetDirDataReference (atr))) &&
	      (ATR_SYNC_DIR_DATA_REFERENCE (ATR_Sync_GetDirDataReference (atr))) > 4 &&
	      (ICC_Sync_GetLength (ps->icc) > 4))
	    {
	      ps->path = 4;
	      ps->length = MIN (ATR_SYNC_DIR_DATA_REFERENCE (ATR_Sync_GetDirDataReference (atr)),
				ICC_Sync_GetLength (ps->icc)) - 4;

	      buffer[0] = 0x90;
	      buffer[1] = 0x00;
	    }
	  
	  else
	    {
	      buffer[0] = 0x6A;
	      buffer[1] = 0x82;
	    }
	}

      else if (fid == PROTOCOL_SYNC_DS_DIR)
	{
	  atr = ICC_Sync_GetAtr (ps->icc);

	  if ((ATR_Sync_GetCategoryIndicator (atr) == ATR_SYNC_CATEGORY_INDICATOR) &&
	      (ATR_SYNC_IS_DIR_DATA_REFERENCE (ATR_Sync_GetDirDataReference (atr))))
	    {
	      if ((tlv_dir = PROTOCOL_SYNC_TLV (ps->icc, ATR_SYNC_DIR_DATA_REFERENCE (ATR_Sync_GetDirDataReference (atr)))))
		{
		  ps->path = TLV_Object_GetAddress (tlv_dir);
		  ps->length = TLV_Object_GetRawLength (tlv_dir);

		  buffer[0] = 0x90;
		  buffer[1] = 0x00;

		  TLV_Object_Delete (tlv_dir);
		}

	      else
		{
		  buffer[0] = 0x6A;
		  buffer[1] = 0x82;
		}
	    }
	  
	  else
	    {
	      buffer[0] = 0x6A;
	      buffer[1] = 0x82;
	    }
	}

      else
	{
	  buffer[0] = 0x6A;
	  buffer[1] = 0x82;
	}
    }

  /* AID selected */
  else if (APDU_Cmd_P1 (cmd) == 0x04)
    {
      /* Get AID */
      aid_length = MIN (APDU_Cmd_Lc (cmd), PROTOCOL_SYNC_AID_SIZE);
      memcpy (aid, APDU_Cmd_Data (cmd), aid_length);

      atr = ICC_Sync_GetAtr (ps->icc);

      if ((ATR_Sync_GetCategoryIndicator (atr) == ATR_SYNC_CATEGORY_INDICATOR) &&
	  (ATR_SYNC_IS_DIR_DATA_REFERENCE (ATR_Sync_GetDirDataReference (atr))))
	{
	  if ((tlv_dir = PROTOCOL_SYNC_TLV (ps->icc, ATR_SYNC_DIR_DATA_REFERENCE (ATR_Sync_GetDirDataReference (atr)))))
	    {
	      /* Mono-application card */
	      if (TLV_Object_GetTag (tlv_dir) == TLV_OBJECT_TAG_APPLICATION_ID)
		{
		  if (TLV_Object_CompareValue (tlv_dir, aid, &aid_length))
		    {
		      if (TLV_Object_Shift (&tlv_dir))
			{
			  ps->path = TLV_Object_GetAddress (tlv_dir);
			  ps->length = TLV_Object_GetRawLength (tlv_dir);

			  buffer[0] = 0x90;
			  buffer[1] = 0x00;

                          TLV_Object_Delete (tlv_dir);
			}

		      /* Cannot initialize app data section */
		      else
			{
			  buffer[0] = 0x6A;
			  buffer[1] = 0x82;
			}
		    }

		  /* Cannot find application ID */
		  else
		    {
		      buffer[0] = 0x6A;
		      buffer[1] = 0x82;

		      TLV_Object_Delete (tlv_dir);
		    }

		}

	      /* Mono-application card with template DO */
	      else if (TLV_Object_GetTag (tlv_dir) == TLV_OBJECT_TAG_TEMPLATE)
		{
		  if ((tlv_aid = TLV_Object_GetObjectByTag (tlv_dir, TLV_OBJECT_TAG_APPLICATION_ID)))
		    {
		      if (TLV_Object_CompareValue (tlv_aid, aid, &aid_length))
			{
			  if (TLV_Object_Shift (&tlv_dir))
			    {
			      ps->path = TLV_Object_GetAddress (tlv_dir);
			      ps->length = TLV_Object_GetRawLength (tlv_dir);

			      buffer[0] = 0x90;
			      buffer[1] = 0x00;

			      TLV_Object_Delete (tlv_dir);
			    }

			  /* Cannot initialize app data section */
			  else
			    {
			      buffer[0] = 0x6A;
			      buffer[1] = 0x82;
			    }
			}

		      /* Cannot find application ID */
		      else
			{
			  buffer[0] = 0x6A;
			  buffer[1] = 0x82;

			  TLV_Object_Delete (tlv_dir);
			}

		      TLV_Object_Delete (tlv_aid);
		    }

		  /* Cannot find application ID data object */
		  else
		    {
		      buffer[0] = 0x6A;
		      buffer[1] = 0x82;

		      TLV_Object_Delete (tlv_dir);
		    }

		}

	      /* Multi-application card */
	      else if (TLV_Object_GetTag (tlv_dir) == TLV_OBJECT_TAG_SEQUENCE)
		{
		  tlv_app = NULL;
		  tlv_template = NULL;

		  /* Iterate over dir data section sequence */
		  while ((!tlv_app) && (TLV_Object_Iterate (tlv_dir, &tlv_template)))
		    {
		      if ((tlv_aid = TLV_Object_GetObjectByTag (tlv_template, TLV_OBJECT_TAG_APPLICATION_ID)))
		        {
		          if (TLV_Object_CompareValue (tlv_aid, aid, &aid_length))
			    {
			      if ((tlv_path = TLV_Object_GetObjectByTag (tlv_template, TLV_OBJECT_TAG_PATH)))
			        {
				  if (TLV_Object_GetValue (tlv_path, path, &path_length))
				    {
				      if (path_length < 2)
					tlv_app = PROTOCOL_SYNC_TLV (ps->icc, path[0]);
				      else 
					tlv_app = PROTOCOL_SYNC_TLV (ps->icc, (path[path_length-2] << 8) | path[path_length-1]);
				    }

				  TLV_Object_Delete (tlv_path);
				}
			    }

                          TLV_Object_Delete (tlv_aid);
			}
		    }

		  /* End of secuence was not reached */
		  if (tlv_template != NULL)
		    TLV_Object_Delete (tlv_template);

		  /* Got it! */
		  if (tlv_app != NULL)
	            {
		      ps->path = TLV_Object_GetAddress (tlv_app);
		      ps->length = TLV_Object_GetRawLength (tlv_app);

		      buffer[0] = 0x90;
		      buffer[1] = 0x00;

		      TLV_Object_Delete (tlv_app);
	            }

		  /* Cannot find or cannot initialize app data section */
		  else
		    {
		      buffer[0] = 0x6A;
		      buffer[1] = 0x82;
		    }
		  
		  TLV_Object_Delete (tlv_dir);
		}

	      /* Invalid tag in dir data section */
	      else
		{
		  buffer[0] = 0x6A;
		  buffer[1] = 0x82;
	          
		  TLV_Object_Delete (tlv_dir);
		}
	    }

	  /* Cannot initialize dir data section */
	  else
	    {
	      buffer[0] = 0x6A;
	      buffer[1] = 0x82;
	    }
	}

      /* Cannot find dir data section */
      else
	{
	  buffer[0] = 0x6A;
	  buffer[1] = 0x82;
	}
    }

  /* Bad selection control */
  else
    {
      /* ID not found */
      buffer[0] = 0x6A;
      buffer[1] = 0x82;
    }

  (*rsp) = APDU_Rsp_New (buffer, 2);
  return PROTOCOL_SYNC_OK;
}

static int
Protocol_Sync_ReadBinary (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp)
{
  unsigned offset, available;
  unsigned long expected;
  BYTE *buffer;

  offset = (APDU_Cmd_P1 (cmd) << 8) | APDU_Cmd_P2 (cmd);
  available = MAX ((signed) (ps->length) - (signed) (offset), 0);

  if (APDU_Cmd_Le_Available (cmd))
    expected = available;
  else
    expected = APDU_Cmd_Le (cmd);

  /* Cannot return more than APDU_MAX_RSP_SIZE - 2 data bytes */
  expected = MIN (expected, APDU_MAX_RSP_SIZE - 2);

  if (expected > available)
    {
      /* Get memory for response */
      buffer = (BYTE *) calloc (available + 2, sizeof (BYTE));

      /* Read data */
      if (ICC_Sync_Read (ps->icc, ps->path + offset, available, buffer) != ICC_SYNC_OK)
	{
	  buffer[0] = 0x65;
	  buffer[1] = 0x01;

	  (*rsp) = APDU_Rsp_New (buffer, 2);
	  free (buffer);

	  return PROTOCOL_SYNC_ICC_ERROR;
	}

      buffer[available] = 0x62;
      buffer[available + 1] = 0x82;

      (*rsp) = APDU_Rsp_New (buffer, available + 2);
      free (buffer);
    }

  else
    {
      /* Get memory for response */
      buffer = (BYTE *) calloc (expected + 2, sizeof (BYTE));

      /* Read data */
      if (ICC_Sync_Read (ps->icc, ps->path + offset, expected, buffer) != ICC_SYNC_OK)
	{
	  buffer[0] = 0x65;
	  buffer[1] = 0x01;

	  (*rsp) = APDU_Rsp_New (buffer, 2);
	  free (buffer);

	  return PROTOCOL_SYNC_ICC_ERROR;
	}

      buffer[expected] = 0x90;
      buffer[expected + 1] = 0x00;

      (*rsp) = APDU_Rsp_New (buffer, expected + 2);
      free (buffer);
    }

  return PROTOCOL_SYNC_OK;
}

static int
Protocol_Sync_UpdateBinary (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp)
{
  unsigned available, offset;
  unsigned long provided;
  BYTE buffer[2];
  int ret;

  offset = (APDU_Cmd_P1 (cmd) << 8) | APDU_Cmd_P2 (cmd);
  available = MAX ((signed) (ps->length) - (signed) (offset), 0);
  provided = APDU_Cmd_Lc (cmd);

  /* Write data */
  ret = ICC_Sync_Write (ps->icc, ps->path + offset, MIN (available, provided), APDU_Cmd_Data (cmd));

  /* Read only error */
  if (ret == ICC_SYNC_RO_ERROR)
    {
      buffer[0] = 0x62;
      buffer[1] = 0x00;

      (*rsp) = APDU_Rsp_New (buffer, 2);
      return PROTOCOL_SYNC_OK;
    }

  /* Communication error */
  else if (ret != ICC_SYNC_OK)
    {
      buffer[0] = 0x62;
      buffer[1] = 0x00;

      (*rsp) = APDU_Rsp_New (buffer, 2);
      return PROTOCOL_SYNC_ICC_ERROR;
    }

  /* Wrote less bytes than given */
  if (available < provided)
    {
      buffer[0] = 0x62;
      buffer[1] = 0x00;

      (*rsp) = APDU_Rsp_New (buffer, 2);
      return PROTOCOL_SYNC_OK;
    }

  buffer[0] = 0x90;
  buffer[1] = 0x00;

  (*rsp) = APDU_Rsp_New (buffer, 2);
  return PROTOCOL_SYNC_OK;
}

static int
Protocol_Sync_Verify (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp)
{
  BYTE buffer[2], pin[ICC_SYNC_PIN_SIZE];
  unsigned trials;
  int ret;

  memset (pin, 0x00, ICC_SYNC_PIN_SIZE);
  memcpy (pin, APDU_Cmd_Data (cmd), MIN (APDU_Cmd_Lc (cmd), ICC_SYNC_PIN_SIZE));

  ret = ICC_Sync_EnterPin (ps->icc, pin, &trials);

  /* Verificarion unsuccessfull */
  if (ret == ICC_SYNC_PIN_ERROR)
    {
      buffer[0] = 0x63;
      buffer[1] = (0xC0 | ((BYTE) trials & 0x0F));
    }

  /* ICC blocked */
  else if (ret == ICC_SYNC_BLOCKED_ERROR)
    {
      buffer[0] = 0x69;
      buffer[1] = 0x83;
    }

  /* Communication error */
  else if (ret != ICC_SYNC_OK)
    {
      buffer[0] = 0x63;
      buffer[1] = 0x00;

      (*rsp) = APDU_Rsp_New (buffer, 2);
      return PROTOCOL_SYNC_ICC_ERROR;
    }

  /* Enter PIN OK */
  else
    {
      buffer[0] = 0x90;
      buffer[1] = 0x00;
    }

  (*rsp) = APDU_Rsp_New (buffer, 2);
  return PROTOCOL_SYNC_OK;
}

static int
Protocol_Sync_ChangeVerifyData (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp)
{
  BYTE pin[ICC_SYNC_PIN_SIZE], newpin[ICC_SYNC_PIN_SIZE];
  BYTE buffer[2];
  unsigned trials;
  int ret;

  memcpy (pin, APDU_Cmd_Data (cmd), ICC_SYNC_PIN_SIZE);
  memcpy (newpin, APDU_Cmd_Data (cmd) + ICC_SYNC_PIN_SIZE, ICC_SYNC_PIN_SIZE);

  /* Verify current pin */
  ret = ICC_Sync_EnterPin (ps->icc, pin, &trials);

  /* Verificarion unsuccessfull */
  if (ret == ICC_SYNC_PIN_ERROR)
    {
      buffer[0] = 0x63;
      buffer[1] = (0xC0 | ((BYTE) trials & 0x0F));

      (*rsp) = APDU_Rsp_New (buffer, 2);
      return PROTOCOL_SYNC_OK;
    }

  /* ICC blocked */
  else if (ret == ICC_SYNC_BLOCKED_ERROR)
    {
      buffer[0] = 0x69;
      buffer[1] = 0x83;

      (*rsp) = APDU_Rsp_New (buffer, 2);
      return PROTOCOL_SYNC_OK;
    }

  /* Communication error */
  else if (ret != ICC_SYNC_OK)
    {
      buffer[0] = 0x63;
      buffer[1] = 0x00;

      (*rsp) = APDU_Rsp_New (buffer, 2);
      return PROTOCOL_SYNC_ICC_ERROR;
    }

  /* Change PIN */
  ret = ICC_Sync_ChangePin (ps->icc, newpin);

  if (ret != ICC_SYNC_OK)
    {
      buffer[0] = 0x63;
      buffer[1] = 0x00;

      (*rsp) = APDU_Rsp_New (buffer, 2);
      return PROTOCOL_SYNC_ICC_ERROR;
    }

  buffer[0] = 0x90;
  buffer[1] = 0x00;

  (*rsp) = APDU_Rsp_New (buffer, 2);
  return PROTOCOL_SYNC_OK;
}

static int
Protocol_Sync_BadCommand (Protocol_Sync * ps, APDU_Cmd * cmd, APDU_Rsp ** rsp)
{
  BYTE buffer[2];

  buffer[0] = 0x6E;
  buffer[1] = 0x00;

  (*rsp) = APDU_Rsp_New (buffer, 2);

  return PROTOCOL_SYNC_OK;
}

static bool 
Protocol_Sync_GetData (void * data, unsigned short address, unsigned short length, BYTE * buffer)
{
  return (ICC_Sync_Read ((ICC_Sync *) data,  address, length,  buffer) == ICC_SYNC_OK);
}

static void
Protocol_Sync_Clear (Protocol_Sync * ps)
{
  ps->icc = NULL;
  ps->path = 0;
  ps->length = 0;
}
