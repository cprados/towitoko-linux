/*
    Big bad tester application that runs against CT-API library.
    Handles up to four card-terminal interfaces simultaneously.
    It issues commands for both synchronous and asynchronous cards.
 
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if !defined DEBUG_IO && !defined DEBUG_IFD && !defined DEBUG_ICC && !defined DEBUG_PROTOCOL && !defined DEBUG_CTAPI
/* Define if you want multi-thread. Disabled by default if we are debugging  */
#define MULTI_THREAD 
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "ctapi.h"
#include "ctbcs.h"
#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
#include <pthread.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif


/* Default Class byte */
#define CLASS 0x00

/* Status of each card-terminal */
struct
{
  unsigned short pn;            /* Serial port (1..4), 0 if not assigned */
  char port[16];		/* Descripton of port, printed in prompt */
  int status;                   /* -1 void, 0 sync, 1 async */
  unsigned char atr[33];        /* ATR bytes */
  unsigned short atr_size;      /* ATR size */
  unsigned char cla;		/* Class byte for commands */
#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
  pthread_t thread;             /* Monitoring thread for this ctn */
  pthread_mutex_t mutex;        /* Mutex for accessin this element */
#endif
}
ct_list[4];

/* Menu loop */
void menu_loop (void);

/* Event handlers */
void Initialize (void);
unsigned short Change (unsigned short);
void Close (unsigned short);
void SelectClass (unsigned short);
void SelectFile (unsigned short);
void GetResponse (unsigned short);
void UpdateBinary (unsigned short);
void ReadBinary (unsigned short);
void VerifyKey (unsigned short);
void SendPPS (unsigned short);
void EnterPin (unsigned short);
void ChangePin (unsigned short);
void ReadData (unsigned short);
void WriteData (unsigned short);

#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
/* Asynchronous monitoring thread function */
void *Monitor (void *);
#else
/* Synchronous polling function */
unsigned short GetSmartcard (unsigned short);
#endif

/* Print status */
void PrintReport (void);

/* Read bus and port */
unsigned short ReadPort (char port[16]);

/* Calculate length of memory card */
unsigned GetMemoryLength(unsigned char * atr, unsigned length);

/* Print array of bytes */
void PrintArray (unsigned char * buffer, unsigned length);

int
main (int argc, char *argv[])
{
  unsigned short ctn;

  /* Initialize status of ct's */
  for (ctn = 0; ctn < 4; ctn++)
    {
      ct_list[ctn].pn = 0;
      ct_list[ctn].status = -1;
      ct_list[ctn].cla = CLASS;
      memset (ct_list[ctn].port, 0, 16);
      memset (ct_list[ctn].atr, 0, 33);
      ct_list[ctn].atr_size = 0;
#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
      pthread_mutex_init (&(ct_list[ctn].mutex), NULL);
#endif
    }

  /* Start menu loop */
  menu_loop ();

  /* Close all open ct's */
  for (ctn = 0; ctn < 4; ctn++)
    {
      if (ct_list[ctn].pn != 0)
        {
          Close (ctn);
#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
          pthread_mutex_destroy (&(ct_list[ctn].mutex));
#endif
        }
    }
    
  return 0;
}

void
menu_loop (void)
{
  char option[32];
  unsigned short ctn = 0, i;
  int dummy;

  while (1)
    {
      PrintReport ();
      printf ("Select option:\n");

      /* Common menu */
      printf ("in: Initialize new terminal\n");
      printf ("ch: Change current terminal\n");
      printf ("cl: Close current terminal\n");

      /* Port is open */
      if (ct_list[ctn].pn != 0)
        {
          /* Processor cards menu */
          if (ct_list[ctn].status == 1)
            {
              printf ("sc: Select Class Byte (current: %02X)\n", ct_list[ctn].cla);
              printf ("sf: Select File\n");
              printf ("gr: Get Response\n");
              printf ("vk: Verify Key (Cryptoflex)\n");
              printf ("rb: Read Binary\n");
              printf ("ub: Update Binary\n");
	      printf ("pps: Send PPS\n");
            }

          /* Memory card menu */
          else if (ct_list[ctn].status == 0)
            {
              printf ("ep: Enter PIN\n");
              printf ("cp: Change PIN\n");
              printf ("rd: Read Data\n");
              printf ("wd: Write Data\n");
            }
        }

      printf ("*: Refresh Menu\n");
      printf ("q: Quit\n");

      if (ct_list[ctn].pn != 0)
        printf ("%s$ ", ct_list[ctn].port);
      else
        printf ("$ ");

      /* Get  menu option */
      scanf ("%s", option);
      dummy = getchar ();

      /* Convert input to lowercase */
      for (i = 0; option[i]; i++)
        option[i] = tolower ((int) option[i]);

      /* Common options */
      if (strcmp (option, "q") == 0)
        break;

      else if (strcmp (option, "in") == 0)
        Initialize ();

      else if (strcmp (option, "ch") == 0)
        ctn = Change (ctn);

      else if (strcmp (option, "cl") == 0)
        Close (ctn);

      /* Processor card options */
      if (ct_list[ctn].status == 1)
        {
          if (strcmp (option, "sc") == 0)
            SelectClass (ctn);

	  else if (strcmp (option, "sf") == 0)
            SelectFile (ctn);

          else if (strcmp (option, "gr") == 0)
            GetResponse (ctn);

          else if (strcmp (option, "vk") == 0)
            VerifyKey (ctn);

          else if (strcmp (option, "rb") == 0)
            ReadBinary (ctn);

          else if (strcmp (option, "ub") == 0)
            UpdateBinary (ctn);

	  else if (strcmp (option, "pps") == 0)
	    SendPPS (ctn);
        }

      /* Memory card options */
      else if (ct_list[ctn].status == 0)
        {
          if (strcmp (option, "ep") == 0)
            EnterPin (ctn);

          else if (strcmp (option, "cp") == 0)
            ChangePin (ctn);

          else if (strcmp (option, "rd") == 0)
            ReadData (ctn);

          else if (strcmp (option, "wd") == 0)
            WriteData (ctn);
        }
    }
}

void
PrintReport (void)
{
  unsigned short ctn, num, i;

  num = 0;
  for (ctn = 0; ctn < 4; ctn++)
    if (ct_list[ctn].pn != 0)
      num++;

  printf
   
 ("**********************************************************************\n");
  printf ("Towitoko CT-API tester utility\n");
  printf ("Copyright (C) 2000 2001 Carlos Prados <cprados@yahoo.com>\n");
  printf ("Initilized CardTerminals: %d\n", num);

  for (ctn = 0; ctn < 4; ctn++)
    {
      if (ct_list[ctn].pn != 0)
        {
#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
          pthread_mutex_lock (&(ct_list[ctn].mutex));
#endif
          printf
           
 ("**********************************************************************\n");
          printf ("%s\n", ct_list[ctn].port);

          printf ("Status: %s",
                  ct_list[ctn].status == 0 ? "Memory smartcard present" :
                  ct_list[ctn].status == 1 ? "Processor smartcard present" :
                  "No smartcard present (type * to refresh)");

	  if (ct_list[ctn].status == 0)
		  printf (". Memory size: %d bytes\n", 
		  GetMemoryLength(ct_list[ctn].atr, ct_list[ctn].atr_size));
	  else
		  printf("\n");

          if (ct_list[ctn].status != -1)
            {
              printf ("ATR: ");
              for (i = 0; i < ct_list[ctn].atr_size; i++)
                printf ("%02X ", ct_list[ctn].atr[i]);
              printf ("\n");
            }
#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
          pthread_mutex_unlock (&(ct_list[ctn].mutex));
#endif
        }
    }

  printf
   
 ("**********************************************************************\n");
}

#if !defined HAVE_PTHREAD_H || !defined MULTI_THREAD
unsigned short
GetSmartcard (unsigned short ctn)
{
  unsigned char cmd[11], res[256], sad, dad;
  unsigned short lr, count;
  char ret;
#ifdef HAVE_NANOSLEEP
  struct timespec req_ts;

  req_ts.tv_sec = 1;
  req_ts.tv_nsec = 0;
#endif

  for (count = 0; count < 60; count ++)
    {
      /* Check if card is inserted */
      cmd[0] = CTBCS_CLA;
      cmd[1] = CTBCS_INS_STATUS;
      cmd[2] = CTBCS_P1_CT_KERNEL;
      cmd[3] = CTBCS_P2_STATUS_ICC;
      cmd[4] = 0x00;

      dad = 1;
      sad = 2;
      lr = 256;

      ret = CT_data (ctn, &dad, &sad, 11, cmd, &lr, res);

      if ((ret != OK) || (res[lr-2] != 0x90))
        {
          fprintf (stderr, "\nError getting status of terminal: %d\n", ret);
          return 0;
        }
 
      if (res[0] == CTBCS_DATA_STATUS_CARD_CONNECT)
        {            
          if (count > 1)                
            printf("\n");
            
          printf ("Activating card...\n");

          /* Activate card */
          cmd[0] = CTBCS_CLA;
          cmd[1] = CTBCS_INS_REQUEST;
          cmd[2] = CTBCS_P1_INTERFACE1;
          cmd[3] = CTBCS_P2_REQUEST_GET_ATR;
          cmd[4] = 0x00;

          dad = 0x01;
          sad = 0x02;
          lr = 256;

          ret = CT_data (ctn, &dad, &sad, 5, cmd, &lr, res);

          if ((ret != OK) || (res[lr - 2] != 0x90))
            {
              fprintf (stderr, "Error activating card: %d\n", ret);
              return 0;
            }

          /* Store the type of card */
          ct_list[ctn].status = res[lr - 1];

          /* Store ATR */
          memcpy (ct_list[ctn].atr, res, lr - 2);
          ct_list[ctn].atr_size = lr - 2;
          return 1;
        }
        
      else
        {
          if (count > 0)
            {
              printf (".");
              fflush (stdout);
            }
          else
            printf ("Please insert a smartcard in the terminal\n");
        }
#ifdef HAVE_NANOSLEEP
          nanosleep (&req_ts, NULL);
#else
          usleep (999999);
#endif                                                                          
 
    }

  printf ("\nTimeout waiting for smartcard insertion\n");
  return 0;
}

#else

void *
Monitor (void *arg)
{
  unsigned char cmd[5], res[256], sad, dad;
  unsigned short lr, ctn;
  char ret;
#ifdef HAVE_NANOSLEEP
  struct timespec req_ts;

  req_ts.tv_sec = 1;
  req_ts.tv_nsec = 0;
#endif

  ctn = *(unsigned short *) arg;
  free (arg);

  while (ct_list[ctn].pn != 0)
    {
      /* Check if card is inserted */
      cmd[0] = CTBCS_CLA;
      cmd[1] = CTBCS_INS_STATUS;
      cmd[2] = CTBCS_P1_CT_KERNEL;
      cmd[3] = CTBCS_P2_STATUS_ICC;
      cmd[4] = 0x00;

      dad = 1;
      sad = 2;
      lr = 256;

      ret = CT_data (ctn, &dad, &sad, 5, cmd, &lr, res);

      if ((ret != OK) || (res[lr-2] != 0x90))
        continue;
      
      pthread_mutex_lock (&(ct_list[ctn].mutex));
      
      if (res[0] == CTBCS_DATA_STATUS_CARD_CONNECT)
        {            
          if (ct_list[ctn].status == -1)
            {
              /* Activate card */
              cmd[0] = CTBCS_CLA;
              cmd[1] = CTBCS_INS_REQUEST;
              cmd[2] = CTBCS_P1_INTERFACE1;
              cmd[3] = CTBCS_P2_REQUEST_GET_ATR;
              cmd[4] = 0x00;

              dad = 0x01;
              sad = 0x02;
              lr = 256;

              ret = CT_data (ctn, &dad, &sad, 5, cmd, &lr, res);

              if ((ret != OK) || (res[lr - 2] != 0x90))
                {
                  pthread_mutex_unlock (&(ct_list[ctn].mutex));
                  continue;
                }   
              
              /* Store the type of card */
              ct_list[ctn].status = res[lr - 1];

              /* Store ATR */
              memcpy (ct_list[ctn].atr, res, lr - 2);
              ct_list[ctn].atr_size = lr - 2;
              
              pthread_mutex_unlock (&(ct_list[ctn].mutex));
            }
        }

      else
        {
          if (ct_list[ctn].status != -1)
            {
              ct_list[ctn].status = -1;
              memset (ct_list[ctn].atr, 0, 33);
              ct_list[ctn].atr_size = 0;
            }
        }
      
      pthread_mutex_unlock (&(ct_list[ctn].mutex));
#ifdef HAVE_NANOSLEEP
      nanosleep (&req_ts, NULL);
#else
      usleep(999999);
#endif
    }

  return NULL;
}
#endif

unsigned short
ReadPort(char port[16])
{
  unsigned short pn;

  scanf("%6s", port);

  if(strncasecmp(port,"COM",3) == 0)
  {
	  sscanf(port+3,"%hu",&pn);
	  snprintf(port,16,"COM%hu",pn);
  }

  else if (strncasecmp(port,"USB",3) == 0)
  {
	  sscanf(port+3,"%hu",&pn);
	  snprintf(port,16,"USB%hu",pn);
	  pn += 32768;
  }
  else
  {
	  sscanf(port,"%hu",&pn);
	  snprintf(port,16,"COM%hu",pn);
  }

  return pn;
}

unsigned 
GetMemoryLength(unsigned char * atr, unsigned length)
{
	if (length < 2)
		return 0;

	/* This line is dedicated to Rene Puls ;-) */
	return( 1 << (((int)(atr[1] & 120) >> 3)+6) * 1 << (int)(atr[1] & 7) / 8);
}

void 
PrintArray (unsigned char * buffer, unsigned length)
{
	unsigned i;

	if (length > 16)
		printf ("\n");

	for (i=0; i<length; i++)
	{
		printf ("%02X ", buffer[i]);
		if (i%16 == 15)
			printf ("\n");
	}

	if (i%16 != 0)
		printf ("\n");
}

void
Initialize (void)
{
  unsigned short int pn, ctn;
  char port[16];
  char ret;

  printf ("Port (COM[1..n] or USB[1..n]): ");

  pn = ReadPort(port);

  for (ctn = 0; ctn < 4; ctn++)
    {
      if ((ct_list[ctn].pn == pn))
        {
          printf ("Port already open\n");
          return;
        }
      else if (ct_list[ctn].pn == 0)
        break;
    }

  if (ctn > 4)
    {
      printf ("Maximum number of ports open\n");
      return;
    }

  printf ("Initializing terminal at %s...\n",port);

#ifndef CTAPI_WIN32_COM
  ret = CT_init (ctn, pn - 1);
#else
  ret = CT_init (ctn, pn);
#endif

  if (ret != OK)
    {
      fprintf (stderr, "Error on port allocation: %d\n", ret);
      return;
    }

  strncpy(ct_list[ctn].port,port,16);
  ct_list[ctn].pn = pn;
  
#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
    {
      unsigned short *arg;

      arg = (unsigned short *) malloc (sizeof (unsigned short));
      (*arg) = ctn;

      printf ("Starting terminal monitoring job...\n");

      pthread_create (&(ct_list[ctn].thread), NULL, Monitor, (void *) arg);

      {
        /* Wait 0.1 seconds before returning to the menu loop
          to give time the monitor thread to adquire the mutex */

#ifdef HAVE_NANOSLEEP
        struct timespec req_ts;

        req_ts.tv_sec = 0;
        req_ts.tv_nsec = 100000000;

        nanosleep (&req_ts, NULL);
#else
        usleep(100000);
#endif
      }
    }
#else
  if (!GetSmartcard (ctn))
    Close (ctn);
#endif
}

unsigned short
Change (unsigned short ctn)
{
  unsigned short pn, i;
  char port[16];

  /* Show the list of open ports */
  printf ("Port number (");

  for (i = 0; i < 4; i++)
    {
      if (ct_list[i].pn != 0)
        printf ("%s", ct_list[i].port);
      if (i + 1 < 4)
        {
          if (ct_list[i + 1].pn != 0)
            printf (", ");
        }
      else
        printf ("): ");
    }

  pn = ReadPort(port);

  /* Search the ctn */
  for (i = 0; i < 4; i++)
    {
      if (ct_list[i].pn == pn)
        break;
    }

  if (i >= 4)
    {
      printf ("Invalid port number\n");
      return ctn;
    }

  return i;
}

void
Close (unsigned short ctn)
{
  char ret;

  if ((ct_list[ctn].pn == 0) && (ct_list[ctn].status == -1))
  {
	  printf ("Port not open\n");
	  return;
  }

#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
  pthread_mutex_lock (&(ct_list[ctn].mutex));
#endif
  ct_list[ctn].pn = 0;
  ct_list[ctn].status = -1;
  memset (ct_list[ctn].atr, 0, 33);
  ct_list[ctn].atr_size = 0;
#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
  pthread_mutex_unlock (&(ct_list[ctn].mutex));
  
  printf ("Waiting for terminal monitoring job to stop...\n");
  pthread_join(ct_list[ctn].thread ,NULL);
#endif

  printf ("Closing terminal at %s\n", ct_list[ctn].port);

  ret = CT_close (ctn);
  if (ret != OK)
    printf ("Error closing terminal at %s\n", ct_list[ctn].port);
}

void
SelectClass (unsigned short ctn)
{
  unsigned char buffer[32];
  int dummy;

  printf ("Class byte (current is %02X): ", ct_list[ctn].cla);
  scanf ("%X", (unsigned int *) buffer);
  dummy = getchar ();

#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
    pthread_mutex_lock (&(ct_list[ctn].mutex));
#endif
  ct_list[ctn].cla = buffer[0];
#if defined HAVE_PTHREAD_H && defined MULTI_THREAD
  pthread_mutex_unlock (&(ct_list[ctn].mutex));
#endif
}

void
SelectFile (unsigned short ctn)
{
  unsigned char select_file[8] = { CLASS, 0xA4, 0x00, 0x00, 0x02, 0x3f, 0x00, 0x00 };
  unsigned char buffer[32];
  unsigned char res[256];
  unsigned char dad;
  unsigned char sad;
  unsigned short lr;
  int dummy;
  char ret;

  select_file[0] = ct_list[ctn].cla;

  printf ("File ID: ");
  scanf ("%X %X", (unsigned int *) buffer, (unsigned int *) (buffer + 1));
  dummy = getchar ();

  select_file[5] = buffer[0];
  select_file[6] = buffer[1];

  dad = 0;
  sad = 2;
  lr = 256;

  printf ("Command: ");
  PrintArray (select_file, 8);

  ret = CT_data (ctn, &dad, &sad, 8, select_file, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error in SELECT FILE: %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);
}

void
GetResponse (unsigned short ctn)
{
  unsigned char get_response[5] = { CLASS, 0xC0, 0x00, 0x00, 0x00 };
  char buffer[32];
  unsigned char res[256];
  unsigned char dad;
  unsigned char sad;
  unsigned short lr;
  int dummy;
  char ret;

  get_response[0] = ct_list[ctn].cla;

  printf ("Response size (hexadecimal): ");
  scanf ("%X", (unsigned int *) buffer);
  dummy = getchar ();
  get_response[4] = buffer[0];

  dad = 0;
  sad = 2;
  lr = 256;

  printf ("Command: ");
  PrintArray (get_response, 5);

  ret = CT_data (ctn, &dad, &sad, 5, get_response, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error on GET RESPONSE: %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);
}

void
UpdateBinary (unsigned short ctn)
{
  unsigned char update_binary[260] = { CLASS, 0xD6, 0x00, 0x00, 0x00 };
  char buffer[32];
  unsigned char res[256];
  unsigned char dad;
  unsigned char sad;
  unsigned short lr;
  int size, dummy;
  char ret;

  update_binary[0] =  ct_list[ctn].cla;

  printf ("File size (0..255): ");
  scanf ("%d", &size);
  dummy = getchar ();
  update_binary[4] = (unsigned char) (size % 256);

  printf ("Data: ");
  scanf ("%02X", (unsigned int *) buffer);
  dummy = getchar ();

  memset(update_binary + 5, buffer[0], update_binary[4]);

  dad = 0;
  sad = 2;
  lr = 256;

  printf ("Command: ");
  PrintArray (update_binary, update_binary[4] + 5);

  ret = CT_data (ctn, &dad, &sad, (int) update_binary[4] + 5, update_binary, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error on UPDATE BYNARY: %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);
}

void
ReadBinary (unsigned short ctn)
{
  unsigned char read_binary[5] = { CLASS, 0xB0, 0x00, 0x00, 0x00 };
  char buffer[32];
  unsigned char res[258];
  unsigned char dad;
  unsigned char sad;
  unsigned short lr;
  int dummy;
  char ret;

  read_binary[0] = ct_list[ctn].cla;

  /* Read binary */
  printf ("File size: ");
  scanf ("%X", (unsigned int *) buffer);
  dummy = getchar ();
  read_binary[4] = buffer[0];

  dad = 0;
  sad = 2;
  lr = 258;

  printf ("Command: ");
  PrintArray (read_binary, 5);

  ret = CT_data (ctn, &dad, &sad, 5, read_binary, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error on READ BYNARY: %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);
}

void
VerifyKey (unsigned short ctn)
{
  unsigned char verify_key[13] =
    { CLASS, 0x2A, 0x00, 0x01, 0x08, 0x47, 0x46, 0x58, 0x49, 0x32, 0x56, 0x78, 0x40 };
  unsigned char res[256];
  unsigned char dad;
  unsigned char sad;
  unsigned short lr;
  char ret;

  verify_key[0] = ct_list[ctn].cla;

  dad = 0;
  sad = 2;
  lr = 256;
  printf ("Command: ");
  PrintArray (verify_key, 13);

  ret = CT_data (ctn, &dad, &sad, 13, verify_key, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error on VERIFY KEY: %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);
}

void
SendPPS (unsigned short ctn)
{
  unsigned char reset[9] = {0x20, 0x11, 0x01, 0x01, 0x03, 0xFF, 0x00, 0x00, 0x00};
  unsigned char buffer[32], res[256];
  unsigned char dad, sad;
  unsigned short lr;
  int dummy;
  char ret;

  printf ("PPS request (PPSS PPS0 PPS1): ");
  scanf ("%X %X %X", (unsigned int *) buffer, (unsigned int *) (buffer + 1), (unsigned int *) (buffer + 2));
  dummy = getchar ();

  reset[5] = buffer[0];
  reset[6] = buffer[1];
  reset[7] = buffer[2];

  dad = 0x01;
  sad = 0x02;
  lr = 256;

  printf ("Command: ");
  PrintArray (reset, 8);

  ret = CT_data (ctn, &dad, &sad, 9, reset, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error on Reset CT (Send  PPS): %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);
}

void
EnterPin (unsigned short ctn)
{
  unsigned char verify[8] = { 0x00, 0x20, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00 };
  unsigned char buffer[32];
  unsigned char res[256];
  unsigned char dad;
  unsigned char sad;
  unsigned short lr;
  int dummy;
  char ret;

  printf ("PIN (3 bytes): ");
  scanf ("%X %X %X", (unsigned int *) buffer,  (unsigned int *) (buffer + 1), (unsigned int *) (buffer + 2));
  dummy = getchar ();

  verify[5] = buffer[0];
  verify[6] = buffer[1];
  verify[7] = buffer[2];

  dad = 0;
  sad = 2;
  lr = 256;

  printf ("Command: ");
  PrintArray (verify, 8);

  ret = CT_data (ctn, &dad, &sad, 8, verify, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error on VERIFY: %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);
}

void
ChangePin (unsigned short ctn)
{
  unsigned char change[11] =
    { 0x00, 0x24, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  unsigned char buffer[32];
  unsigned char res[256];
  unsigned char dad;
  unsigned char sad;
  unsigned short lr;
  int dummy;
  char ret;

  printf ("PIN (3 bytes): ");
  scanf ("%X %X %X", (unsigned int *) buffer, (unsigned int *) (buffer + 1), (unsigned int *) (buffer + 2));
  dummy = getchar ();

  change[5] = buffer[0];
  change[6] = buffer[1];
  change[7] = buffer[2];

  printf ("New PIN (3 bytes): ");
  scanf ("%X %X %X", (unsigned int *) buffer, (unsigned int *) (buffer + 1), (unsigned int *) (buffer + 2));
  dummy = getchar ();

  change[8] = buffer[0];
  change[9] = buffer[1];
  change[10] = buffer[2];

  dad = 0;
  sad = 2;
  lr = 256;

  printf ("Command: ");
  PrintArray (change, 11);

  ret = CT_data (ctn, &dad, &sad, 11, change, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error on CHANGE VERIFICATION DATA: %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);
}

void
ReadData (unsigned short ctn)
{
  unsigned char select_file[7] = { 0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
  unsigned char read_binary[6] = { 0x00, 0xB0, 0x00, 0x00, 0x00, 0x00 };
  int address;
  int size, total_size;
  unsigned char * res;
  unsigned char dad;
  unsigned char sad;
  unsigned short lr, lc;
  int dummy;
  char ret;

  printf ("Address: ");
  scanf ("%d", &address);
  dummy = getchar ();

  read_binary[2] = (unsigned char) (address >> 8);
  read_binary[3] = (unsigned char) (address & 0x00FF);

  total_size  = GetMemoryLength(ct_list[ctn].atr, ct_list[ctn].atr_size);

  printf ("Size (0..%d): ", total_size);
  scanf ("%d", &size);
  dummy = getchar ();

  if (size < 256)
  {
	read_binary[4] = (unsigned char) size;
	lc = 5;
  }
  else
  {
	read_binary[4] = 0;
	read_binary[5] = (unsigned char) (size >> 8);
	read_binary[6] = (unsigned char) (size & 0x00FF);
	lc = 7;
  }

  printf ("Command: ");
  PrintArray (select_file, 7);

  dad = 0;
  sad = 2;
  res  = calloc ((lr = 2), sizeof (unsigned char));

  /* Select MF */
  ret = CT_data (ctn, &dad, &sad, 7, select_file, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error on SELECT FILE: %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);

  free (res);

  printf ("Command: ");
  PrintArray (read_binary, lc);

  dad = 0;
  sad = 2;
  if (size != 0)
  	res = calloc ((lr = size + 2), sizeof (unsigned char));
  else
	res  = calloc ((lr = total_size + 2), sizeof (unsigned char));

  if (res == NULL)
	  return;

  /* Read binary */
  ret = CT_data (ctn, &dad, &sad, lc, read_binary, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error on READ BINARY: %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);

  free  (res);
}

void
WriteData (unsigned short ctn)
{
  unsigned char select_file[7] = { 0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
  unsigned char * update_binary;
  int address;
  int size, total_size;
  unsigned char buffer[32];
  unsigned char res[256];
  unsigned char dad;
  unsigned char sad;
  unsigned short lr, lc;
  int dummy;
  char ret;

  printf ("Address: ");
  scanf ("%d", &address);
  dummy = getchar ();

  total_size  = GetMemoryLength(ct_list[ctn].atr, ct_list[ctn].atr_size);

  printf ("Size (0..%d): ", total_size);
  scanf ("%d", &size);
  dummy = getchar ();

  printf ("Data: ");
  scanf ("%X", (unsigned int *) buffer);
  dummy = getchar ();

  if  (size < 256)
  {
	update_binary = calloc (size +5, sizeof (unsigned char));

	if (update_binary == NULL)
		return;

	update_binary[0] = 0x00;
	update_binary[1] = 0xD6;
	update_binary[2] = (unsigned char) (address >> 8);
	update_binary[3] = (unsigned char) (address & 0x00FF);
	update_binary[4] = size;
	memset (update_binary+5, buffer[0], size);
	lc = size + 5;
  }
  else
  {
	update_binary = calloc (size +7, sizeof (unsigned char));

	if (update_binary == NULL)
		return;

	update_binary[0] = 0x00;
	update_binary[1] = 0xD6;
	update_binary[2] = (unsigned char) (address >> 8);
	update_binary[3] = (unsigned char) (address & 0x00FF);
	update_binary[4] = 0;
	update_binary[5] = (unsigned char) (size >> 8);
	update_binary[6] = (unsigned char) (size & 0x00FF);
	memset (update_binary+7, buffer[0], size);
	lc = size +7;
  }

  printf ("Command: ");
  PrintArray (select_file, 7);

  dad = 0;
  sad = 2;
  lr = 256;

  /* Select MF */
  ret = CT_data (ctn, &dad, &sad, 7, select_file, &lr, res);

  if (ret != OK)
    {
      fprintf (stderr, "Error on SELECT FILE: %d\n", ret);
      free (update_binary);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);

  printf ("Command: ");
  PrintArray (update_binary, lc);

  dad = 0;
  sad = 2;
  lr = 256;

  /* Update binary */
  ret = CT_data (ctn, &dad, &sad, lc, update_binary, &lr, res);

  free (update_binary);

  if (ret != OK)
    {
      fprintf (stderr, "Error on UPDATE BINARY: %d\n", ret);
      return;
    }

  printf ("Response: ");
  PrintArray (res, lr);
}

