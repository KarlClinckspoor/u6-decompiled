/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 0676: the credits, starting GAME.EXE
 */
#include "u.h"
#include <alloc.h>
#include <process.h>

/*a drawing surface of the graphics driver (14 bytes)*/

extern int D_5044;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern struct tSurface D_5710;/*the screen*/
extern struct tSurface D_5732;
extern struct tSurface D_565F;/*the console's surface*/
extern struct tSurface *D_566D;/*the console: surface it writes on*/
extern int D_566F;/*the console: x*/
extern int D_5671;/*the console: y*/
extern char D_5677, D_5678;/*the console: text colours*/
extern char D_567D;
extern struct tSurface D_5702;
extern void far *D_5AD6;/*the vellum background picture*/
extern struct tSurface D_6072;
extern char D_5696[];/*GAME.EXE's command line argument*/

extern SetPalette();
extern PollInput();/*nonzero when the player wants to go on (key or time)*/
extern AskForDisk(int);/*AskForDisk*/
extern SURF_alloc(struct tSurface *);/*allocates an off-screen buffer (and row table) of a surface's size*/
extern SURF_free(struct tSurface *);/*frees them*/
extern TXT_clear();
extern SURF_share(struct tSurface *, struct tSurface *, int);
extern TXT_sprintf(char far *, ...);/*console printf*/
extern TXT_sappend(char far *);/*console print*/
extern VID_setMode(int);
extern FADE_in(struct tSurface *, struct tSurface *);/*MCGA: palette fade in*/
extern FADE_out(struct tSurface *, int);/*MCGA: palette fade out*/
extern void far *LZW_load(char *);/*loads a file in memory*/

/*C_0676_0006*/Credits() {
	struct tSurface save;
	struct tSurface box;
	struct tSurface box2;
	struct tSurface tmp;

	if(D_5044 == 0xd) {
		DRV(0xf)(&D_5732, &D_5710);
		D_5732 = D_5710;
	}
	box = D_5732;
	box2 = D_5710;
	/*the text area on the vellum*/
	box.right = box2.right = 0x124;
	box.left = box2.left = 0x1c;
	box.top = box2.top = 0x59;
	box.bottom = box2.bottom = 0xbd;
	D_5702 = D_565F;
	SURF_share(&D_5732, &D_5702, 0);
	DRV(0xc)(&D_5702, 0);
	/*save the screen*/
	save.right = D_565F.right - D_565F.left;
	save.bottom = D_565F.bottom - D_565F.top;
	save.left = save.top = 0;
	SURF_alloc(&save);
	switch(D_5044) {
		case 4: D_5AD6 = LZW_load("vellum1.cga"); break;
		case 7: D_5AD6 = LZW_load("vellum1.hrc"); break;
		case 9: D_5AD6 = LZW_load("vellum1.tga"); break;
		case 0xd: D_5AD6 = LZW_load("vellum1.ega"); break;
		case 0x13: D_5AD6 = LZW_load("vellum1.shp"); break;
	}
	if(D_5044 == 0xd)
		D_565F.segment = D_5710.segment ^ 0x200;
	DRV(0xf)(&D_565F, &save);
	if(D_5044 == 0x13)
		FADE_out(&D_565F, 0);
	DRV(0xc)(&D_565F, 0);
	SetPalette();
	if(D_5044 == 0xd)
		D_5702.segment = D_5710.segment;
	/*draw the vellum*/
	DRV(3)(&D_5702, D_5702.left, D_5702.top, D_5AD6, 0);
	D_566D = &box;
	switch(D_5044) {
		case 4: case 7: D_5677 = 0; D_5678 = 3; break;
		case 9: case 0xd: D_5677 = 0; D_5678 = 0xe; break;
		case 0x13: D_5677 = 0x3d; D_5678 = 0x31; break;
	}
	D_566F = box.left;
	D_5671 = box.top;
	D_567D = 2;
	TXT_sprintf("Ultima VI\nA Lord British Production%P");
	/*a line under the title*/
	tmp = box;
	tmp.top = D_5671 + 9;
	tmp.bottom = D_5671 + 9;
	switch(D_5044) {
		case 4: case 7: case 9: case 0xd: DRV(0xc)(&tmp, 0); break;
		case 0x13: DRV(0xc)(&tmp, 0x3d); break;
	}
	box.top = D_5671 + 0xe;
	box2.top = D_5671 + 0xe;
	if(D_5044 == 0xd) {
		tmp = box;
		tmp.segment = D_5710.segment;
		tmp.pScanlines = D_5710.pScanlines;
		D_566D = &tmp;
		D_5732.segment = D_5710.segment;
	}
	/*page 1*/
	TXT_clear();
	D_5671++;
	TXT_sprintf("%FProduced by\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Richard Garriott and Warren Spector\n\n%P");
	TXT_sprintf("%FExecutive Producer\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Dallas Snell\n\n%P");
	TXT_sprintf("%FProgramming\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Cheryl Chen    John Miles\n%P");
	TXT_sprintf("Herman Miller    Gary Scott Smith%P");
	if(D_5044 == 0x13) {
		FADE_in(&D_5702, &D_565F);
	} else if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
		DRV(0xf)(&D_5732, &D_5710);
		tmp.segment = D_5710.segment;
	} else {
		DRV(0xf)(&D_5702, &D_565F);
	}
	TXT_clear();
	TXT_sprintf("%F\n\nWriting\n%P%F", 0xc, 0x3d);
	TXT_sprintf("Stephen Beeman    Dr. Cat    'Manda Dee\n");
	TXT_sappend("Richard Garriott    Greg Malone\n");
	TXT_sappend("John Miles    Herman Miller\n");
	TXT_sappend("Todd Porter    Warren Spector\n%P");
	while(!PollInput());
	/*page 2*/
	if(D_5044 != 0xd) {
		DRV(0xf)(&box, &box2);
	} else {
		DRV(0x15)(&D_5710);
		tmp.segment = D_5710.segment;
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
	if(D_5044 != 0xd) {
		DRV(0xf)(&box, &box2);
	} else {
		DRV(0x15)(&D_5710);
		tmp.segment = D_5710.segment;
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
	if(D_5044 != 0xd) {
		DRV(0xf)(&box, &box2);
	} else {
		D_565F.segment = D_5710.segment;
		DRV(0x15)(&D_5710);
	}
	while(!PollInput());
	/*put the screen back*/
	farfree(D_5AD6);
	if(D_5044 == 0x13)
		FADE_out(&D_565F, 0);
	DRV(0xc)(&D_565F, 0);
	SetPalette();
	if(D_5044 == 0x13) {
		FADE_in(&save, &D_565F);
	} else {
		DRV(0xf)(&save, &D_565F);
	}
	SURF_free(&save);
}

/*C_0676_0699*/StartGame() {
	int i;

	D_6072.left = D_6072.top = 0;
	D_6072.right = 0x13f;
	D_6072.bottom = 0x37;
	if(D_5044 != 0xd)
		SURF_alloc(&D_6072);
	else
		D_6072.segment = 0xa600;
	AskForDisk(4);
	if(D_5044 != 0xd)
		SURF_free(&D_6072);
	if(D_5044 == 0x13)
		FADE_out(&D_5710, 0);
	VID_setMode(3);
	if(D_5044 == 9) {
		/*Tandy: add a '$' to the argument*/
		for(i = 0; D_5696[i]; i++);
		D_5696[i] = '$';
		D_5696[i + 1] = 0;
	}
	for(;;)
		execl("game.exe", "", D_5696, 0);
}
