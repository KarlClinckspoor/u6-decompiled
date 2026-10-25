/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 0676: the credits, starting GAME.EXE
 */
#include "u.h"
#include <alloc.h>
#include <process.h>

/*a drawing surface of the graphics driver (14 bytes)*/

extern int16 videoMode;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern struct tSurface screen;/*the screen*/
extern struct tSurface backBuffer;
extern struct tSurface textArea;/*the console's surface*/
extern struct tSurface textAreaCopy;
extern void far *vellum;/*the vellum background picture*/
extern struct tSurface workArea;
extern char gameArgs[];/*GAME.EXE's command line argument*/

extern SetPalette();
extern int16 PollInput();/*nonzero when the player wants to go on (key or time)*/
extern AskForDisk(int16 n);/*AskForDisk*/
extern SURF_alloc(struct tSurface *);/*allocates an off-screen buffer (and row table) of a surface's size*/
extern SURF_free(struct tSurface *);/*frees them*/
extern TXT_clear();
extern SURF_share(struct tSurface *, struct tSurface *, int16);
extern TXT_sprintf(char far *, ...);/*console printf*/
extern TXT_sappend(char far *);/*console print*/
extern VID_setMode(int16);
extern FADE_in(struct tSurface *, struct tSurface *);/*MCGA: palette fade in*/
extern FADE_out(struct tSurface *, int16);/*MCGA: palette fade out*/
extern void far *LZW_load(char *);/*loads a file in memory*/

/*C_0676_0006*/Credits() {
	struct tSurface save;
	struct tSurface box;
	struct tSurface box2;
	struct tSurface tmp;

	if(videoMode == 0xd) {
		DRV(0xf)(&backBuffer, &screen);
		backBuffer = screen;
	}
	box = backBuffer;
	box2 = screen;
	/*the text area on the vellum*/
	box.right = box2.right = 0x124;
	box.left = box2.left = 0x1c;
	box.top = box2.top = 0x59;
	box.bottom = box2.bottom = 0xbd;
	textAreaCopy = textArea;
	SURF_share(&backBuffer, &textAreaCopy, 0);
	DRV(0xc)(&textAreaCopy, 0);
	/*save the screen*/
	save.right = textArea.right - textArea.left;
	save.bottom = textArea.bottom - textArea.top;
	save.left = save.top = 0;
	SURF_alloc(&save);
	switch(videoMode) {
		case 4: vellum = LZW_load("vellum1.cga"); break;
		case 7: vellum = LZW_load("vellum1.hrc"); break;
		case 9: vellum = LZW_load("vellum1.tga"); break;
		case 0xd: vellum = LZW_load("vellum1.ega"); break;
		case 0x13: vellum = LZW_load("vellum1.shp"); break;
	}
	if(videoMode == 0xd)
		textArea.segment = screen.segment ^ 0x200;
	DRV(0xf)(&textArea, &save);
	if(videoMode == 0x13)
		FADE_out(&textArea, 0);
	DRV(0xc)(&textArea, 0);
	SetPalette();
	if(videoMode == 0xd)
		textAreaCopy.segment = screen.segment;
	/*draw the vellum*/
	DRV(3)(&textAreaCopy, textAreaCopy.left, textAreaCopy.top, vellum, 0);
	textWin = &box;
	switch(videoMode) {
		case 4: case 7: textFg = 0; textBg = 3; break;
		case 9: case 0xd: textFg = 0; textBg = 0xe; break;
		case 0x13: textFg = 0x3d; textBg = 0x31; break;
	}
	textX = box.left;
	textY = box.top;
	textAlign = 2;
	TXT_sprintf("Ultima VI\nA Lord British Production%P");
	/*a line under the title*/
	tmp = box;
	tmp.top = textY + 9;
	tmp.bottom = textY + 9;
	switch(videoMode) {
		case 4: case 7: case 9: case 0xd: DRV(0xc)(&tmp, 0); break;
		case 0x13: DRV(0xc)(&tmp, 0x3d); break;
	}
	box.top = textY + 0xe;
	box2.top = textY + 0xe;
	if(videoMode == 0xd) {
		tmp = box;
		tmp.segment = screen.segment;
		tmp.pScanlines = screen.pScanlines;
		textWin = &tmp;
		backBuffer.segment = screen.segment;
	}
	/*page 1*/
	TXT_clear();
	textY++;
	TXT_sprintf("%FProduced by\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Richard Garriott and Warren Spector\n\n%P");
	TXT_sprintf("%FExecutive Producer\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Dallas Snell\n\n%P");
	TXT_sprintf("%FProgramming\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Cheryl Chen    John Miles\n%P");
	TXT_sprintf("Herman Miller    Gary Scott Smith%P");
	if(videoMode == 0x13) {
		FADE_in(&textAreaCopy, &textArea);
	} else if(videoMode == 0xd) {
		DRV(0x15)(&screen);
		DRV(0xf)(&backBuffer, &screen);
		tmp.segment = screen.segment;
	} else {
		DRV(0xf)(&textAreaCopy, &textArea);
	}
	TXT_clear();
	TXT_sprintf("%F\n\nWriting\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Stephen Beeman    Dr. Cat    'Manda Dee\n");
	TXT_sappend("Richard Garriott    Greg Malone\n");
	TXT_sappend("John Miles    Herman Miller\n");
	TXT_sappend("Todd Porter    Warren Spector\n%P");
	while(!PollInput());
	/*page 2*/
	if(videoMode != 0xd) {
		DRV(0xf)(&box, &box2);
	} else {
		DRV(0x15)(&screen);
		tmp.segment = screen.segment;
	}
	TXT_clear();
	TXT_sprintf("%FArt\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Keith Berdak    Daniel Bourbonnais\n");
	TXT_sappend("Jeff Dee    'Manda Dee\n");
	TXT_sappend("Glen Johnson    Denis Loubet\n\n%P");
	TXT_sprintf("%FMusic\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Ken Arnold    Iolo Fitzowen\n");
	TXT_sappend("Herman Miller    Todd Porter\n%P");
	while(!PollInput());
	/*page 3*/
	if(videoMode != 0xd) {
		DRV(0xf)(&box, &box2);
	} else {
		DRV(0x15)(&screen);
		tmp.segment = screen.segment;
	}
	TXT_clear();
	TXT_sprintf("%FQuality Assurance\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Paul Malone    Mike Romero\n");
	TXT_sappend("Marc Schaefgen    John Watson\n\n%P");
	TXT_sprintf("%FAdditional Support\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Michelle Caddel    Melanie Fleming\n");
	TXT_sappend("Alan Gardner    Jeff Hillhouse\n");
	TXT_sappend("Sherry Hunter    Steve Muchow\n");
	TXT_sappend("Cheryl Neeld%P");
	while(!PollInput());
	if(videoMode != 0xd) {
		DRV(0xf)(&box, &box2);
	} else {
		textArea.segment = screen.segment;
		DRV(0x15)(&screen);
	}
	while(!PollInput());
	/*put the screen back*/
	farfree(vellum);
	if(videoMode == 0x13)
		FADE_out(&textArea, 0);
	DRV(0xc)(&textArea, 0);
	SetPalette();
	if(videoMode == 0x13) {
		FADE_in(&save, &textArea);
	} else {
		DRV(0xf)(&save, &textArea);
	}
	SURF_free(&save);
}

/*C_0676_0699*/StartGame() {
	int16 i;

	workArea.left = workArea.top = 0;
	workArea.right = 0x13f;
	workArea.bottom = 0x37;
	if(videoMode != 0xd)
		SURF_alloc(&workArea);
	else
		workArea.segment = 0xa600;
	AskForDisk(4);
	if(videoMode != 0xd)
		SURF_free(&workArea);
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	VID_setMode(3);
	if(videoMode == 9) {
		/*Tandy: add a '$' to the argument*/
		for(i = 0; gameArgs[i]; i++);
		gameArgs[i] = '$';
		gameArgs[i + 1] = 0;
	}
	for(;;)
		execl("game.exe", "", gameArgs, 0);
}
