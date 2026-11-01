/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __OSI_FILE_H__
#define __OSI_FILE_H__

/*error management */
/*032E*/extern union tHandler pascal OSI_ErrorHandler;/*far callback invoked by OSI file functions on error*/
/*0332*/extern int16 pascal OSI_ErrorCode;/*DOS error code, -1 short read/write; handler sets 0 to retry*/
/*0334*/extern int16 pascal OSI_ErrorFunc;/*failing function: 1 open 2 close 3 create 4 delete 5 read 6 write 7 rename 8 copy*/
/*0336*/extern char * pascal OSI_ErrorFile;/*last file name passed to open/create/delete/rename*/
/* */

extern pascal OSI_callback();

extern int16 pascal OSI_open(char *path);
extern int16 pascal OSI_close(int16 handle);
extern int16 pascal OSI_create(char *path);
extern uint32 pascal OSI_read(int16 handle, uint32 offset, uint32 length, void far *buf);
extern uint32 pascal OSI_write(int16 handle, uint32 offset, uint32 length, void far *buf);
extern uint32 pascal OSI_fileSize(int16 handle);


/*TODO*/

#endif
