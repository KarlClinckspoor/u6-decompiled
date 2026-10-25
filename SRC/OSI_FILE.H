/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __OSI_FILE_H__
#define __OSI_FILE_H__

/*error management */
/*032E*/extern union tHandler pascal OSI_ErrorHandler;/*far callback invoked by OSI file functions on error*/
/*0332*/extern int pascal OSI_ErrorCode;/*DOS error code, -1 short read/write; handler sets 0 to retry*/
/*0334*/extern int pascal OSI_ErrorFunc;/*failing function: 1 open 2 close 3 create 4 delete 5 read 6 write 7 rename 8 copy*/
/*0336*/extern char * pascal OSI_ErrorFile;/*last file name passed to open/create/delete/rename*/
/* */

extern pascal OSI_callback();

extern int pascal OSI_open(char *path);
extern int pascal OSI_close(int handle);
extern int pascal OSI_create(char *path);
extern unsigned long pascal OSI_read(int handle, unsigned long offset, unsigned long length, void far *buf);
extern unsigned long pascal OSI_write(int handle, unsigned long offset, unsigned long length, void far *buf);
extern unsigned long pascal OSI_fileSize(int handle);


/*TODO*/

#endif
