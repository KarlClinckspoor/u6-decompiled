/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 0676: the credits, starting GAME.EXE
 */
#include "u.h"
#include <alloc.h>
#include <process.h>

/*a drawing surface of the graphics driver (14 bytes)*/
struct t14 {
	unsigned seg;/*_00*//*video memory segment (or of an off-screen buffer)*/
	unsigned far *ytable;/*_02*//*offset of each row*/
	int right;/*_06*/
	int left;/*_08*/
	int top;/*_0A*/
	int bottom;/*_0C*/
};

extern int D_5044;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern struct t14 D_5710;/*the screen*/
extern struct t14 D_5732;
extern struct t14 D_565F;/*the console's surface*/
extern struct t14 *D_566D;/*the console: surface it writes on*/
extern int D_566F;/*the console: x*/
extern int D_5671;/*the console: y*/
extern char D_5677, D_5678;/*the console: text colours*/
extern char D_567D;
extern struct t14 D_5702;
extern void far *D_5AD6;/*the vellum background picture*/
extern struct t14 D_6072;
extern char D_5696[];/*GAME.EXE's command line argument*/

extern C_00E1_04F9();
extern C_06E9_0351();/*nonzero when the player wants to go on (key or time)*/
extern C_0BC1_0079(int);/*AskForDisk*/
extern C_1277_000C(struct t14 *);/*allocates an off-screen buffer (and row table) of a surface's size*/
extern C_1277_00D8(struct t14 *);/*frees them*/
extern C_1288_0002();
extern C_12EA_000A(struct t14 *, struct t14 *, int);
extern C_12EE_0249(char far *, ...);/*console printf*/
extern C_12EE_0277(char far *);/*console print*/
extern C_1336_001E(int);
extern C_136D_014D(struct t14 *, struct t14 *);/*MCGA: palette fade in*/
extern C_136D_0346(struct t14 *, int);/*MCGA: palette fade out*/
extern void far *C_13BC_000C(char *);/*loads a file in memory*/

/*C_0676_0006*/Credits() {
	struct t14 save;
	struct t14 box;
	struct t14 box2;
	struct t14 tmp;

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
	C_12EA_000A(&D_5732, &D_5702, 0);
	DRV(0xc)(&D_5702, 0);
	/*save the screen*/
	save.right = D_565F.right - D_565F.left;
	save.bottom = D_565F.bottom - D_565F.top;
	save.left = save.top = 0;
	C_1277_000C(&save);
	switch(D_5044) {
		case 4: D_5AD6 = C_13BC_000C("vellum1.cga"); break;
		case 7: D_5AD6 = C_13BC_000C("vellum1.hrc"); break;
		case 9: D_5AD6 = C_13BC_000C("vellum1.tga"); break;
		case 0xd: D_5AD6 = C_13BC_000C("vellum1.ega"); break;
		case 0x13: D_5AD6 = C_13BC_000C("vellum1.shp"); break;
	}
	if(D_5044 == 0xd)
		D_565F.seg = D_5710.seg ^ 0x200;
	DRV(0xf)(&D_565F, &save);
	if(D_5044 == 0x13)
		C_136D_0346(&D_565F, 0);
	DRV(0xc)(&D_565F, 0);
	C_00E1_04F9();
	if(D_5044 == 0xd)
		D_5702.seg = D_5710.seg;
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
	C_12EE_0249("Ultima VI\nA Lord British Production%P");
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
		tmp.seg = D_5710.seg;
		tmp.ytable = D_5710.ytable;
		D_566D = &tmp;
		D_5732.seg = D_5710.seg;
	}
	/*page 1*/
	C_1288_0002();
	D_5671++;
	C_12EE_0249("%FProduced by\n%P%F", 0xc, 0x3d);
	C_12EE_0249("Richard Garriott and Warren Spector\n\n%P");
	C_12EE_0249("%FExecutive Producer\n%P%F", 0xc, 0x3d);
	C_12EE_0249("Dallas Snell\n\n%P");
	C_12EE_0249("%FProgramming\n%P%F", 0xc, 0x3d);
	C_12EE_0249("Cheryl Chen    John Miles\n%P");
	C_12EE_0249("Herman Miller    Gary Scott Smith%P");
	if(D_5044 == 0x13) {
		C_136D_014D(&D_5702, &D_565F);
	} else if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
		DRV(0xf)(&D_5732, &D_5710);
		tmp.seg = D_5710.seg;
	} else {
		DRV(0xf)(&D_5702, &D_565F);
	}
	C_1288_0002();
	C_12EE_0249("%F\n\nWriting\n%P%F", 0xc, 0x3d);
	C_12EE_0249("Stephen Beeman    Dr. Cat    'Manda Dee\n");
	C_12EE_0277("Richard Garriott    Greg Malone\n");
	C_12EE_0277("John Miles    Herman Miller\n");
	C_12EE_0277("Todd Porter    Warren Spector\n%P");
	while(!C_06E9_0351());
	/*page 2*/
	if(D_5044 != 0xd) {
		DRV(0xf)(&box, &box2);
	} else {
		DRV(0x15)(&D_5710);
		tmp.seg = D_5710.seg;
	}
	C_1288_0002();
	C_12EE_0249("%FArt\n%P%F", 0xc, 0x3d);
	C_12EE_0249("Keith Berdak    Daniel Bourbonnais\n");
	C_12EE_0277("Jeff Dee    'Manda Dee\n");
	C_12EE_0277("Glen Johnson    Denis Loubet\n\n%P");
	C_12EE_0249("%FMusic\n%P%F", 0xc, 0x3d);
	C_12EE_0249("Ken Arnold    Iolo Fitzowen\n");
	C_12EE_0277("Herman Miller    Todd Porter\n%P");
	while(!C_06E9_0351());
	/*page 3*/
	if(D_5044 != 0xd) {
		DRV(0xf)(&box, &box2);
	} else {
		DRV(0x15)(&D_5710);
		tmp.seg = D_5710.seg;
	}
	C_1288_0002();
	C_12EE_0249("%FQuality Assurance\n%P%F", 0xc, 0x3d);
	C_12EE_0249("Paul Malone    Mike Romero\n");
	C_12EE_0277("Marc Schaefgen    John Watson\n\n%P");
	C_12EE_0249("%FAdditional Support\n%P%F", 0xc, 0x3d);
	C_12EE_0249("Michelle Caddel    Melanie Fleming\n");
	C_12EE_0277("Alan Gardner    Jeff Hillhouse\n");
	C_12EE_0277("Sherry Hunter    Steve Muchow\n");
	C_12EE_0277("Cheryl Neeld%P");
	while(!C_06E9_0351());
	if(D_5044 != 0xd) {
		DRV(0xf)(&box, &box2);
	} else {
		D_565F.seg = D_5710.seg;
		DRV(0x15)(&D_5710);
	}
	while(!C_06E9_0351());
	/*put the screen back*/
	farfree(D_5AD6);
	if(D_5044 == 0x13)
		C_136D_0346(&D_565F, 0);
	DRV(0xc)(&D_565F, 0);
	C_00E1_04F9();
	if(D_5044 == 0x13) {
		C_136D_014D(&save, &D_565F);
	} else {
		DRV(0xf)(&save, &D_565F);
	}
	C_1277_00D8(&save);
}

/*C_0676_0699*/StartGame() {
	int i;

	D_6072.left = D_6072.top = 0;
	D_6072.right = 0x13f;
	D_6072.bottom = 0x37;
	if(D_5044 != 0xd)
		C_1277_000C(&D_6072);
	else
		D_6072.seg = 0xa600;
	C_0BC1_0079(4);
	if(D_5044 != 0xd)
		C_1277_00D8(&D_6072);
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	C_1336_001E(3);
	if(D_5044 == 9) {
		/*Tandy: add a '$' to the argument*/
		for(i = 0; D_5696[i]; i++);
		D_5696[i] = '$';
		D_5696[i + 1] = 0;
	}
	for(;;)
		execl("game.exe", "", D_5696, 0);
}
