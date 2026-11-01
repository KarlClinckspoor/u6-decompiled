; Ultima VI - The False Prophet: U.EXE
; the uninitialised data (_BSS) of the C modules, at the addresses U.EXE has them.
; Turbo C 2.0 orders a module's _BSS by its symbol table, not by the order of
; definition, so (as SRC/BSS.ASM does for GAME.EXE) it is laid out here; the C modules
; declare these extern. ORG reserves the room without writing any data, so that
; the uninitialised data stays out of the .EXE as in the original. Each label is followed by the address (DS offset) and what it is.

DGROUP	GROUP	_BSS
_BSS	SEGMENT WORD PUBLIC 'BSS'

PUBLIC	_menuColors,_gfxDrv,_vgaPalette,_videoMode,_mouseInfo,_savedPalette,_textArea,_console
PUBLIC	_songData,_gameArgs,_spareSurface,_textAreaCopy,_screen,_songPos,_musicPort,_conSurface
PUBLIC	_backBuffer,_farMemLeft,_musicCard,_egaSaved,_menuDrawn,_scanlineOfs,_mouseOn,_adapter
PUBLIC	_escaped,_blockShapes,_cursorShapes,_musDrv,_buttonAreas,_oldName,_oldStr
PUBLIC	_oldDex,_oldInt,_oldSex,_oldClass,_oldGood,_oldAvatar,_oldExp,_oldLevel
PUBLIC	_oldHp,_oldHpMax,_oldMp,_dialogArea,_nameWindow,_titleArea,_transferFrom,_sheet
PUBLIC	_portraitIdx,_newName,_newStr,_newDex,_newInt,_newSex,_newClass,_newGood
PUBLIC	_newAvatar,_newExp,_newLevel,_newHp,_newHpMax,_newMp,_newPortrait,_portraits
PUBLIC	_vellum,_saveData,_sceneShapes,_patternArea,_egaPage2,_egaScreen,_patternAdded,_vialLevel
PUBLIC	_patternColorCount,_chosenVirtues,_patternColors,_pattern,_loopPos,_loopCount,_chosenPicture,_palEntry
PUBLIC	_palColors,_palColors2,_skySurface,_skyCopy,_lowerSurface,_introPage2,_cloudCount,_script
PUBLIC	_cloudShapes,_cloudSurface,_rainSurface,_rainDelay,_bankX,_bankY,_scriptPos,_rainX
PUBLIC	_cloudX,_rainY,_cloudY,_cloudShape,_rainWait,_monitor,_rainShape,_cloudSpeed
PUBLIC	_intro1Shapes,_rainChance,_monitorScroll,_intro2Shapes,_rainMax,_monitorNoisy,_rainCount,_workArea
PUBLIC	_frameShape,_frameX,_frameY,_passageArea,_dosMajor,_dosMinor,_twinklePal,_twinkleEntry
PUBLIC	_twinkleEntry2,_starIndex,_starPixel,_starLit,_starPosData,_twinkleFirst,_starCount,_twinkleRandom
PUBLIC	_starColor,_starX,_starY,_hands,_shapeDest,_shapePos

_menuColors	LABEL	WORD	;D_4A14 the 5 main menu highlight colors (r, g, b)
	ORG	$+32
_gfxDrv	LABEL	BYTE	;D_4A34
	ORG	$+12
_vgaPalette	LABEL	WORD	;D_4A40 VGA palette (r, g, b)
	ORG	$+1540
_videoMode	LABEL	WORD	;D_5044 video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA
	ORG	$+2
_mouseInfo	LABEL	BYTE	;D_5046 struct tMouse: the mouse
	ORG	$+25
_savedPalette	LABEL	WORD	;D_505F saved VGA palette
	ORG	$+1536
_textArea	LABEL	BYTE	;D_565F the vellum's area
	ORG	$+14
_console	LABEL	BYTE	;D_566D struct tText: the console's text window
	ORG	$+37
_songData	LABEL	DWORD	;D_5692 the music driver's song
	ORG	$+4
_gameArgs	LABEL	BYTE	;D_5696 GAME.EXE's command line argument
	ORG	$+80
_spareSurface	LABEL	BYTE	;D_56E6
	ORG	$+28
_textAreaCopy	LABEL	BYTE	;D_5702 copy of D_565F
	ORG	$+14
_screen	LABEL	BYTE	;D_5710 the screen
	ORG	$+14
_songPos	LABEL	WORD	;D_571E parse position in midi.dat
	ORG	$+2
_musicPort	LABEL	WORD	;D_5720 music card i/o port
	ORG	$+2
_conSurface	LABEL	BYTE	;D_5722
	ORG	$+16
_backBuffer	LABEL	BYTE	;D_5732 the back buffer
	ORG	$+14
_farMemLeft	LABEL	BYTE	;D_5740 free far memory
	ORG	$+6
_musicCard	LABEL	WORD	;D_5746 music on
	ORG	$+2
_egaSaved	LABEL	BYTE	;D_5748 EGA: the page the noise is drawn into
	ORG	$+18
_menuDrawn	LABEL	WORD	;D_575A set when the player gave up
	ORG	$+2
_scanlineOfs	LABEL	WORD	;D_575C scanline offsets
	ORG	$+400
_mouseOn	LABEL	WORD	;D_58EC mouse on
	ORG	$+2
_adapter	LABEL	WORD	;D_58EE graphic adapter: 0 VGA, 1 EGA, 2 Tandy, 3 CGA, 4 Hercules
	ORG	$+2
_escaped	LABEL	WORD	;D_58F0 1: Escape was pressed: leave the introduction
	ORG	$+2
_blockShapes	LABEL	DWORD	;D_58F2 blocks.xxx
	ORG	$+4
_cursorShapes	LABEL	DWORD	;D_58F6 mouse pointer shapes (cursors.xxx)
	ORG	$+4
_musDrv	LABEL	BYTE	;D_58FA the music driver
	ORG	$+4
_buttonAreas	LABEL	BYTE	;D_58FE the buttons
	ORG	$+70
_oldName	LABEL	BYTE	;D_5944 name
	ORG	$+16
_oldStr	LABEL	WORD	;D_5954 strength, dexterity, intelligence
	ORG	$+2
_oldDex	LABEL	WORD	;D_5956 strength, dexterity, intelligence
	ORG	$+2
_oldInt	LABEL	WORD	;D_5958 strength, dexterity, intelligence
	ORG	$+2
_oldSex	LABEL	BYTE	;D_595A "Male"/"Female"
	ORG	$+8
_oldClass	LABEL	BYTE	;D_5962 class
	ORG	$+16
_oldGood	LABEL	BYTE	;D_5972 'G'
	ORG	$+1
_oldAvatar	LABEL	BYTE	;D_5973 1: an Avatar
	ORG	$+1
_oldExp	LABEL	WORD	;D_5974 experience
	ORG	$+2
_oldLevel	LABEL	WORD	;D_5976 level
	ORG	$+2
_oldHp	LABEL	WORD	;D_5978 hit points, maximum
	ORG	$+2
_oldHpMax	LABEL	WORD	;D_597A hit points, maximum
	ORG	$+2
_oldMp	LABEL	WORD	;D_597C magic points
	ORG	$+4
_dialogArea	LABEL	BYTE	;D_5980
	ORG	$+28
_nameWindow	LABEL	BYTE	;D_599C the text window of the name question
	ORG	$+14
_titleArea	LABEL	BYTE	;D_59AA the title
	ORG	$+14
_transferFrom	LABEL	WORD	;D_59B8 the vellum has been shown
	ORG	$+104
_sheet	LABEL	BYTE	;D_5A20 the character sheet
	ORG	$+14
_portraitIdx	LABEL	WORD	;D_5A2E portrait (0-5)
	ORG	$+2
_newName	LABEL	BYTE	;D_5A30 name
	ORG	$+16
_newStr	LABEL	WORD	;D_5A40 strength, dexterity, intelligence
	ORG	$+2
_newDex	LABEL	WORD	;D_5A42 strength, dexterity, intelligence
	ORG	$+2
_newInt	LABEL	WORD	;D_5A44 strength, dexterity, intelligence
	ORG	$+2
_newSex	LABEL	BYTE	;D_5A46 sex: 'M' or 'F'
	ORG	$+8
_newClass	LABEL	BYTE	;D_5A4E "Avatar"
	ORG	$+16
_newGood	LABEL	BYTE	;D_5A5E 'G'
	ORG	$+1
_newAvatar	LABEL	BYTE	;D_5A5F
	ORG	$+1
_newExp	LABEL	WORD	;D_5A60 experience
	ORG	$+2
_newLevel	LABEL	WORD	;D_5A62 level
	ORG	$+2
_newHp	LABEL	WORD	;D_5A64 hit points
	ORG	$+2
_newHpMax	LABEL	WORD	;D_5A66 hit points, maximum
	ORG	$+2
_newMp	LABEL	WORD	;D_5A68 magic points
	ORG	$+2
_newPortrait	LABEL	BYTE	;D_5A6A portrait
	ORG	$+104
_portraits	LABEL	DWORD	;D_5AD2 montage image
	ORG	$+4
_vellum	LABEL	DWORD	;D_5AD6 vellum image
	ORG	$+4
_saveData	LABEL	BYTE	;D_5ADA the save file read
	ORG	$+2
_sceneShapes	LABEL	BYTE	;D_5ADC woods / gypsy image
	ORG	$+10
_patternArea	LABEL	BYTE	;D_5AE6 the area of the gypsy's pattern
	ORG	$+20
_egaPage2	LABEL	BYTE	;D_5AFA EGA: the second page (segment a800)
	ORG	$+14
_egaScreen	LABEL	BYTE	;D_5B08 EGA: the screen, for the mouse pointer
	ORG	$+14
_patternAdded	LABEL	WORD	;D_5B16
	ORG	$+2
_vialLevel	LABEL	WORD	;D_5B18 level of each vial
	ORG	$+16
_patternColorCount	LABEL	WORD	;D_5B28 number of colors in the pattern
	ORG	$+2
_chosenVirtues	LABEL	WORD	;D_5B2A virtues
	ORG	$+16
_patternColors	LABEL	WORD	;D_5B3A colors of the pattern
	ORG	$+64
_pattern	LABEL	BYTE	;D_5B7A pattern
	ORG	$+32
_loopPos	LABEL	WORD	;D_5B9A script: position of the loop
	ORG	$+2
_loopCount	LABEL	WORD	;D_5B9C script: loop count
	ORG	$+2
_chosenPicture	LABEL	WORD	;D_5B9E script: the picture chosen by command 0x87
	ORG	$+2
_palEntry	LABEL	WORD	;D_5BA0 palette entry
	ORG	$+2
_palColors	LABEL	WORD	;D_5BA2 palette colors
	ORG	$+6
_palColors2	LABEL	WORD	;D_5BA8 palette colors
	ORG	$+6
_skySurface	LABEL	BYTE	;D_5BAE twinkle: the surface the stars are drawn on
	ORG	$+14
_skyCopy	LABEL	BYTE	;D_5BBC
	ORG	$+14
_lowerSurface	LABEL	BYTE	;D_5BCA
	ORG	$+14
_introPage2	LABEL	BYTE	;D_5BD8 EGA: the second page
	ORG	$+14
_cloudCount	LABEL	WORD	;D_5BE6 clouds: their number
	ORG	$+2
_script	LABEL	WORD	;D_5BE8 script: the current one
	ORG	$+2
_cloudShapes	LABEL	DWORD	;D_5BEA clouds: the shapes
	ORG	$+4
_cloudSurface	LABEL	BYTE	;D_5BEE clouds: the surface
	ORG	$+2
_rainSurface	LABEL	BYTE	;D_5BF0 rain: the surface
	ORG	$+2
_rainDelay	LABEL	WORD	;D_5BF2 rain: frames between the moves of a drop
	ORG	$+2
_bankX	LABEL	WORD	;D_5BF4 clouds: x, y of the big cloud bank
	ORG	$+2
_bankY	LABEL	WORD	;D_5BF6 clouds: x, y of the big cloud bank
	ORG	$+2
_scriptPos	LABEL	WORD	;D_5BF8 script: position in it
	ORG	$+2
_rainX	LABEL	WORD	;D_5BFA rain: x, y of each drop
	ORG	$+300
_cloudX	LABEL	WORD	;D_5D26 clouds: x, y, shape of each
	ORG	$+20
_rainY	LABEL	WORD	;D_5D3A rain: x, y of each drop
	ORG	$+300
_cloudY	LABEL	WORD	;D_5E66 clouds: x, y, shape of each
	ORG	$+20
_cloudShape	LABEL	WORD	;D_5E7A clouds: x, y, shape of each
	ORG	$+20
_rainWait	LABEL	WORD	;D_5E8E rain: frames before the next move of each drop
	ORG	$+300
_monitor	LABEL	BYTE	;D_5FBA the monitor's screen
	ORG	$+14
_rainShape	LABEL	BYTE	;D_5FC8 rain: shape of each drop
	ORG	$+150
_cloudSpeed	LABEL	WORD	;D_605E clouds: speed
	ORG	$+2
_intro1Shapes	LABEL	DWORD	;D_6060 shapes of intro1
	ORG	$+4
_rainChance	LABEL	WORD	;D_6064 rain: 1 in n chances of a new drop every frame
	ORG	$+2
_monitorScroll	LABEL	WORD	;D_6066 how far the monitor's picture went out of the screen on the left
	ORG	$+2
_intro2Shapes	LABEL	DWORD	;D_6068 shapes of intro2
	ORG	$+4
_rainMax	LABEL	WORD	;D_606C rain: the maximum number of drops - 1
	ORG	$+2
_monitorNoisy	LABEL	WORD	;D_606E 1: the monitor showed noise: redraw its frame
	ORG	$+2
_rainCount	LABEL	WORD	;D_6070 rain: the number of drops
	ORG	$+2
_workArea	LABEL	BYTE	;D_6072 a 320x56 work surface (the A/B buttons)
	ORG	$+14
_frameShape	LABEL	WORD	;D_6080
	ORG	$+2
_frameX	LABEL	WORD	;D_6082 where the text area is on the screen: x (in 8-pixel columns?), y
	ORG	$+2
_frameY	LABEL	WORD	;D_6084 where the text area is on the screen: x (in 8-pixel columns?), y
	ORG	$+2
_passageArea	LABEL	BYTE	;D_6086 copy of D_5732 the text area is shown in
	ORG	$+14
_dosMajor	LABEL	BYTE	;D_6094 DOS version: major, minor
	ORG	$+1
_dosMinor	LABEL	BYTE	;D_6095 DOS version: major, minor
	ORG	$+1
_twinklePal	LABEL	WORD	;D_6096 twinkle: palette index (a global: the twinkle runs from the timer)
	ORG	$+2
_twinkleEntry	LABEL	BYTE	;D_6098 twinkle: a palette entry
	ORG	$+6
_twinkleEntry2	LABEL	BYTE	;D_609E twinkle: a palette entry
	ORG	$+6
_starIndex	LABEL	WORD	;D_60A4 twinkle: star index
	ORG	$+2
_starPixel	LABEL	WORD	;D_60A6 twinkle: colour of the star's pixel
	ORG	$+2
_starLit	LABEL	WORD	;D_60A8 twinkle: colour of a lit star
	ORG	$+2
_starPosData	LABEL	DWORD	;D_60AA starpos.dat
	ORG	$+4
_twinkleFirst	LABEL	WORD	;D_60AE twinkle: first star of the group that twinkles
	ORG	$+2
_starCount	LABEL	WORD	;D_60B0 number of stars
	ORG	$+2
_twinkleRandom	LABEL	WORD	;D_60B2 twinkle: random number (a shift register)
	ORG	$+10
_starColor	LABEL	WORD	;D_60BC colour of each star
	ORG	$+512
_starX	LABEL	WORD	;D_62BC x of each star
	ORG	$+512
_starY	LABEL	WORD	;D_64BC y of each star
	ORG	$+512
_hands	LABEL	WORD	;D_66BC the hands: x0,y0, x1,y1
	ORG	$+8
_shapeDest	LABEL	WORD	;D_66C4 where the moving shapes go
	ORG	$+16
_shapePos	LABEL	WORD	;D_66D4 the moving shapes: x,y each
	ORG	$+16

_BSS	ENDS

END
