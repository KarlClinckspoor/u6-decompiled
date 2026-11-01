; Ultima VI - The False Prophet: END.EXE
; the uninitialised data (_BSS) of the C modules (main.c, tick.c), at the addresses
; END.EXE has them: laid out here for the reason SRC_U/BSS.ASM gives (Turbo C orders a
; module's _BSS by its symbol table). Each label is followed by its DS offset.

DGROUP	GROUP	_BSS
_BSS	SEGMENT WORD PUBLIC 'BSS'

PUBLIC	_gfxDrv,_endShapes,_leftHalf,_rightHalf,_leftLens,_rightLens,_scrLower,_backLower
PUBLIC	_scrLeft,_backLeft,_scrRight,_backRight,_textSurface,_codexStage,_videoMode,_mouseInfo
PUBLIC	_scene,_starSeedX,_starSeedY,_palette,_console,_songData,_starColor,_screen
PUBLIC	_songPos,_textShape,_scrCodex,_musicPort,_backCodex,_textShapeX,_textShapeY,_backBuffer
PUBLIC	_musicCard,_egaSaved,_textBack,_starX,_starY,_scanlineOfs,_mouseOn,_adapter
PUBLIC	_musDrv,_palIndex,_egaPage,_palTemp,_palSaved

_gfxDrv	LABEL	BYTE	;1770 union tDrv: the graphics driver
	ORG	$+4
_endShapes	LABEL	DWORD	;1774 the shapes of the ending (far)
	ORG	$+4
_leftHalf	LABEL	BYTE	;1778 struct tSurface
	ORG	$+14
_rightHalf	LABEL	BYTE	;1786 struct tSurface
	ORG	$+14
_leftLens	LABEL	BYTE	;1794 struct tSurface
	ORG	$+14
_rightLens	LABEL	BYTE	;17A2 struct tSurface
	ORG	$+14
_scrLower	LABEL	BYTE	;17B0 struct tSurface
	ORG	$+14
_backLower	LABEL	BYTE	;17BE struct tSurface
	ORG	$+14
_scrLeft	LABEL	BYTE	;17CC struct tSurface
	ORG	$+14
_backLeft	LABEL	BYTE	;17DA struct tSurface
	ORG	$+14
_scrRight	LABEL	BYTE	;17E8 struct tSurface
	ORG	$+14
_backRight	LABEL	BYTE	;17F6 struct tSurface
	ORG	$+14
_textSurface	LABEL	BYTE	;1804 struct tSurface
	ORG	$+14
_codexStage	LABEL	WORD	;1812 
	ORG	$+4
_videoMode	LABEL	WORD	;1816 video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA
	ORG	$+2
_mouseInfo	LABEL	BYTE	;1818 struct tMouse: the mouse
	ORG	$+25
_scene	LABEL	WORD	;1831 the scene of the ending (0..15)
	ORG	$+2
_starSeedX	LABEL	WORD	;1833 random generator of the stars' x
	ORG	$+2
_starSeedY	LABEL	WORD	;1835 random generator of the stars' y
	ORG	$+2
_palette	LABEL	WORD	;1837 [256][3] the VGA palette
	ORG	$+1536
_console	LABEL	BYTE	;1E37 struct tText: the console
	ORG	$+17
_songData	LABEL	DWORD	;1E48 the song being played (far)
	ORG	$+4
_starColor	LABEL	BYTE	;1E4C [64] the stars' colours
	ORG	$+64
_screen	LABEL	BYTE	;1E8C struct tSurface: the screen
	ORG	$+14
_songPos	LABEL	WORD	;1E9A 
	ORG	$+2
_textShape	LABEL	WORD	;1E9C 
	ORG	$+2
_scrCodex	LABEL	BYTE	;1E9E struct tSurface
	ORG	$+14
_musicPort	LABEL	WORD	;1EAC 
	ORG	$+2
_backCodex	LABEL	BYTE	;1EAE struct tSurface
	ORG	$+14
_textShapeX	LABEL	WORD	;1EBC 
	ORG	$+2
_textShapeY	LABEL	WORD	;1EBE 
	ORG	$+2
_backBuffer	LABEL	BYTE	;1EC0 struct tSurface
	ORG	$+14
_musicCard	LABEL	WORD	;1ECE 
	ORG	$+2
_egaSaved	LABEL	BYTE	;1ED0 struct tSurface
	ORG	$+14
_textBack	LABEL	BYTE	;1EDE struct tSurface
	ORG	$+14
_starX	LABEL	BYTE	;1EEC [64] the stars' x
	ORG	$+64
_starY	LABEL	BYTE	;1F2C [64] the stars' y
	ORG	$+64
_scanlineOfs	LABEL	WORD	;1F6C [200] the offset of each line on the screen
	ORG	$+400
_mouseOn	LABEL	WORD	;20FC 
	ORG	$+2
_adapter	LABEL	WORD	;20FE 
	ORG	$+2
_musDrv	LABEL	BYTE	;2100 union tMus: the music driver
	ORG	$+4
_palIndex	LABEL	WORD	;2104 (tick.c) the colour being moved by the cycling
	ORG	$+2
_egaPage	LABEL	WORD	;2106 (tick.c) EGA: the page the stars are drawn on
	ORG	$+2
_palTemp	LABEL	WORD	;2108 (tick.c) [3] the colour being moved
	ORG	$+6
_palSaved	LABEL	WORD	;210E (tick.c) [3] colour 31, which goes round to 16
	ORG	$+6

_BSS	ENDS

END
