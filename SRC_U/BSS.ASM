; Ultima VI - The False Prophet: U.EXE
; the uninitialised data (_BSS) of the C modules, at the addresses U.EXE has them.
; Turbo C 2.0 orders a module's _BSS by its symbol table, not by the order of
; definition, so (as SRC/BSS.ASM does for GAME.EXE) it is laid out here; the C modules
; declare these extern. ORG reserves the room without writing any data, so that
; the uninitialised data stays out of the .EXE as in the original. Each label is followed by the address (DS offset) and what it is.

DGROUP	GROUP	_BSS
_BSS	SEGMENT WORD PUBLIC 'BSS'

PUBLIC	_D_4A14,_D_4A34,_D_4A40,_D_5044,_D_5046,_D_5047,_D_5048,_D_504A
PUBLIC	_D_504E,_D_5052,_D_5054,_D_5056,_D_505E,_D_505F,_D_565F,_D_566D
PUBLIC	_D_566F,_D_5671,_D_5673,_D_5677,_D_5678,_D_5679,_D_567D,_D_5692
PUBLIC	_D_5696,_D_56E6,_D_5702,_D_5710,_D_571E,_D_5720,_D_5722,_D_5732
PUBLIC	_D_5740,_D_5746,_D_5748,_D_575A,_D_575C,_D_58EC,_D_58EE,_D_58F0
PUBLIC	_D_58F2,_D_58F4,_D_58F6,_D_58FA,_D_58FE,_D_5944,_D_5954,_D_5956
PUBLIC	_D_5958,_D_595A,_D_5962,_D_5972,_D_5973,_D_5974,_D_5976,_D_5978
PUBLIC	_D_597A,_D_597C,_D_5980,_D_599C,_D_59AA,_D_59B8,_D_5A20,_D_5A2E
PUBLIC	_D_5A30,_D_5A40,_D_5A42,_D_5A44,_D_5A46,_D_5A4E,_D_5A5E,_D_5A5F
PUBLIC	_D_5A60,_D_5A62,_D_5A64,_D_5A66,_D_5A68,_D_5A6A,_D_5AD2,_D_5AD6
PUBLIC	_D_5ADA,_D_5ADC,_D_5AE6,_D_5AFA,_D_5B08,_D_5B16,_D_5B18,_D_5B28
PUBLIC	_D_5B2A,_D_5B3A,_D_5B7A,_D_5B9A,_D_5B9C,_D_5B9E,_D_5BA0,_D_5BA2
PUBLIC	_D_5BA8,_D_5BAE,_D_5BBC,_D_5BCA,_D_5BD8,_D_5BE6,_D_5BE8,_D_5BEA
PUBLIC	_D_5BEE,_D_5BF0,_D_5BF2,_D_5BF4,_D_5BF6,_D_5BF8,_D_5BFA,_D_5D26
PUBLIC	_D_5D3A,_D_5E66,_D_5E7A,_D_5E8E,_D_5FBA,_D_5FC8,_D_605E,_D_6060
PUBLIC	_D_6064,_D_6066,_D_6068,_D_606C,_D_606E,_D_6070,_D_6072,_D_6080
PUBLIC	_D_6082,_D_6084,_D_6086,_D_6094,_D_6095,_D_6096,_D_6098,_D_609E
PUBLIC	_D_60A4,_D_60A6,_D_60A8,_D_60AA,_D_60AE,_D_60B0,_D_60B2,_D_60BC
PUBLIC	_D_62BC,_D_64BC,_D_66BC,_D_66C4,_D_66D4

_D_4A14	LABEL	WORD	;4A14 the 5 main menu highlight colors (r, g, b)
	ORG	$+32
_D_4A34	LABEL	BYTE	;4A34 
	ORG	$+12
_D_4A40	LABEL	WORD	;4A40 VGA palette (r, g, b)
	ORG	$+1540
_D_5044	LABEL	WORD	;5044 video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA
	ORG	$+2
_D_5046	LABEL	BYTE	;5046 mouse buttons
	ORG	$+1
_D_5047	LABEL	BYTE	;5047 mouse buttons
	ORG	$+1
_D_5048	LABEL	WORD	;5048 mouse x, y
	ORG	$+2
_D_504A	LABEL	WORD	;504A mouse x, y
	ORG	$+4
_D_504E	LABEL	DWORD	;504E mouse pointer shapes
	ORG	$+4
_D_5052	LABEL	WORD	;5052 mouse pointer shape
	ORG	$+2
_D_5054	LABEL	BYTE	;5054 the surface the mouse pointer is drawn on
	ORG	$+2
_D_5056	LABEL	BYTE	;5056 mouse background save area
	ORG	$+8
_D_505E	LABEL	BYTE	;505E a mouse is present?
	ORG	$+1
_D_505F	LABEL	WORD	;505F saved VGA palette
	ORG	$+1536
_D_565F	LABEL	BYTE	;565F the vellum's area
	ORG	$+14
_D_566D	LABEL	BYTE	;566D the current text window
	ORG	$+2
_D_566F	LABEL	WORD	;566F text cursor x, y
	ORG	$+2
_D_5671	LABEL	WORD	;5671 text cursor x, y
	ORG	$+2
_D_5673	LABEL	DWORD	;5673 console: font
	ORG	$+4
_D_5677	LABEL	BYTE	;5677 text colors
	ORG	$+1
_D_5678	LABEL	BYTE	;5678 text colors
	ORG	$+1
_D_5679	LABEL	BYTE	;5679 console: buffer
	ORG	$+4
_D_567D	LABEL	BYTE	;567D the console: alignment (0 left, 1 right, 2 centred)
	ORG	$+21
_D_5692	LABEL	DWORD	;5692 the music driver's song
	ORG	$+4
_D_5696	LABEL	BYTE	;5696 GAME.EXE's command line argument
	ORG	$+80
_D_56E6	LABEL	BYTE	;56E6 
	ORG	$+28
_D_5702	LABEL	BYTE	;5702 copy of D_565F
	ORG	$+14
_D_5710	LABEL	BYTE	;5710 the screen
	ORG	$+14
_D_571E	LABEL	WORD	;571E parse position in midi.dat
	ORG	$+2
_D_5720	LABEL	WORD	;5720 music card i/o port
	ORG	$+2
_D_5722	LABEL	BYTE	;5722 
	ORG	$+16
_D_5732	LABEL	BYTE	;5732 the back buffer
	ORG	$+14
_D_5740	LABEL	BYTE	;5740 free far memory
	ORG	$+6
_D_5746	LABEL	WORD	;5746 music on
	ORG	$+2
_D_5748	LABEL	BYTE	;5748 EGA: the page the noise is drawn into
	ORG	$+18
_D_575A	LABEL	WORD	;575A set when the player gave up
	ORG	$+2
_D_575C	LABEL	WORD	;575C scanline offsets
	ORG	$+400
_D_58EC	LABEL	WORD	;58EC mouse on
	ORG	$+2
_D_58EE	LABEL	WORD	;58EE graphic adapter: 0 VGA, 1 EGA, 2 Tandy, 3 CGA, 4 Hercules
	ORG	$+2
_D_58F0	LABEL	WORD	;58F0 1: Escape was pressed: leave the introduction
	ORG	$+2
_D_58F2	LABEL	BYTE	;58F2 blocks.xxx
	ORG	$+2
_D_58F4	LABEL	WORD	;58F4 
	ORG	$+2
_D_58F6	LABEL	DWORD	;58F6 mouse pointer shapes (cursors.xxx)
	ORG	$+4
_D_58FA	LABEL	BYTE	;58FA the music driver
	ORG	$+4
_D_58FE	LABEL	BYTE	;58FE the buttons
	ORG	$+70
_D_5944	LABEL	BYTE	;5944 name
	ORG	$+16
_D_5954	LABEL	WORD	;5954 strength, dexterity, intelligence
	ORG	$+2
_D_5956	LABEL	WORD	;5956 strength, dexterity, intelligence
	ORG	$+2
_D_5958	LABEL	WORD	;5958 strength, dexterity, intelligence
	ORG	$+2
_D_595A	LABEL	BYTE	;595A "Male"/"Female"
	ORG	$+8
_D_5962	LABEL	BYTE	;5962 class
	ORG	$+16
_D_5972	LABEL	BYTE	;5972 'G'
	ORG	$+1
_D_5973	LABEL	BYTE	;5973 1: an Avatar
	ORG	$+1
_D_5974	LABEL	WORD	;5974 experience
	ORG	$+2
_D_5976	LABEL	WORD	;5976 level
	ORG	$+2
_D_5978	LABEL	WORD	;5978 hit points, maximum
	ORG	$+2
_D_597A	LABEL	WORD	;597A hit points, maximum
	ORG	$+2
_D_597C	LABEL	WORD	;597C magic points
	ORG	$+4
_D_5980	LABEL	BYTE	;5980 
	ORG	$+28
_D_599C	LABEL	BYTE	;599C the text window of the name question
	ORG	$+14
_D_59AA	LABEL	BYTE	;59AA the title
	ORG	$+14
_D_59B8	LABEL	WORD	;59B8 the vellum has been shown
	ORG	$+104
_D_5A20	LABEL	BYTE	;5A20 the character sheet
	ORG	$+14
_D_5A2E	LABEL	WORD	;5A2E portrait (0-5)
	ORG	$+2
_D_5A30	LABEL	BYTE	;5A30 name
	ORG	$+16
_D_5A40	LABEL	WORD	;5A40 strength, dexterity, intelligence
	ORG	$+2
_D_5A42	LABEL	WORD	;5A42 strength, dexterity, intelligence
	ORG	$+2
_D_5A44	LABEL	WORD	;5A44 strength, dexterity, intelligence
	ORG	$+2
_D_5A46	LABEL	BYTE	;5A46 sex: 'M' or 'F'
	ORG	$+8
_D_5A4E	LABEL	BYTE	;5A4E "Avatar"
	ORG	$+16
_D_5A5E	LABEL	BYTE	;5A5E 'G'
	ORG	$+1
_D_5A5F	LABEL	BYTE	;5A5F 
	ORG	$+1
_D_5A60	LABEL	WORD	;5A60 experience
	ORG	$+2
_D_5A62	LABEL	WORD	;5A62 level
	ORG	$+2
_D_5A64	LABEL	WORD	;5A64 hit points
	ORG	$+2
_D_5A66	LABEL	WORD	;5A66 hit points, maximum
	ORG	$+2
_D_5A68	LABEL	WORD	;5A68 magic points
	ORG	$+2
_D_5A6A	LABEL	BYTE	;5A6A portrait
	ORG	$+104
_D_5AD2	LABEL	DWORD	;5AD2 montage image
	ORG	$+4
_D_5AD6	LABEL	DWORD	;5AD6 vellum image
	ORG	$+4
_D_5ADA	LABEL	BYTE	;5ADA the save file read
	ORG	$+2
_D_5ADC	LABEL	BYTE	;5ADC woods / gypsy image
	ORG	$+10
_D_5AE6	LABEL	BYTE	;5AE6 the area of the gypsy's pattern
	ORG	$+20
_D_5AFA	LABEL	BYTE	;5AFA EGA: the second page (segment a800)
	ORG	$+14
_D_5B08	LABEL	BYTE	;5B08 EGA: the screen, for the mouse pointer
	ORG	$+14
_D_5B16	LABEL	WORD	;5B16 
	ORG	$+2
_D_5B18	LABEL	WORD	;5B18 level of each vial
	ORG	$+16
_D_5B28	LABEL	WORD	;5B28 number of colors in the pattern
	ORG	$+2
_D_5B2A	LABEL	WORD	;5B2A virtues
	ORG	$+16
_D_5B3A	LABEL	WORD	;5B3A colors of the pattern
	ORG	$+64
_D_5B7A	LABEL	BYTE	;5B7A pattern
	ORG	$+32
_D_5B9A	LABEL	WORD	;5B9A script: position of the loop
	ORG	$+2
_D_5B9C	LABEL	WORD	;5B9C script: loop count
	ORG	$+2
_D_5B9E	LABEL	WORD	;5B9E script: the picture chosen by command 0x87
	ORG	$+2
_D_5BA0	LABEL	WORD	;5BA0 palette entry
	ORG	$+2
_D_5BA2	LABEL	WORD	;5BA2 palette colors
	ORG	$+6
_D_5BA8	LABEL	WORD	;5BA8 palette colors
	ORG	$+6
_D_5BAE	LABEL	BYTE	;5BAE twinkle: the surface the stars are drawn on
	ORG	$+14
_D_5BBC	LABEL	BYTE	;5BBC 
	ORG	$+14
_D_5BCA	LABEL	BYTE	;5BCA 
	ORG	$+14
_D_5BD8	LABEL	BYTE	;5BD8 EGA: the second page
	ORG	$+14
_D_5BE6	LABEL	WORD	;5BE6 clouds: their number
	ORG	$+2
_D_5BE8	LABEL	WORD	;5BE8 script: the current one
	ORG	$+2
_D_5BEA	LABEL	DWORD	;5BEA clouds: the shapes
	ORG	$+4
_D_5BEE	LABEL	BYTE	;5BEE clouds: the surface
	ORG	$+2
_D_5BF0	LABEL	BYTE	;5BF0 rain: the surface
	ORG	$+2
_D_5BF2	LABEL	WORD	;5BF2 rain: frames between the moves of a drop
	ORG	$+2
_D_5BF4	LABEL	WORD	;5BF4 clouds: x, y of the big cloud bank
	ORG	$+2
_D_5BF6	LABEL	WORD	;5BF6 clouds: x, y of the big cloud bank
	ORG	$+2
_D_5BF8	LABEL	WORD	;5BF8 script: position in it
	ORG	$+2
_D_5BFA	LABEL	WORD	;5BFA rain: x, y of each drop
	ORG	$+300
_D_5D26	LABEL	WORD	;5D26 clouds: x, y, shape of each
	ORG	$+20
_D_5D3A	LABEL	WORD	;5D3A rain: x, y of each drop
	ORG	$+300
_D_5E66	LABEL	WORD	;5E66 clouds: x, y, shape of each
	ORG	$+20
_D_5E7A	LABEL	WORD	;5E7A clouds: x, y, shape of each
	ORG	$+20
_D_5E8E	LABEL	WORD	;5E8E rain: frames before the next move of each drop
	ORG	$+300
_D_5FBA	LABEL	BYTE	;5FBA the monitor's screen
	ORG	$+14
_D_5FC8	LABEL	BYTE	;5FC8 rain: shape of each drop
	ORG	$+150
_D_605E	LABEL	WORD	;605E clouds: speed
	ORG	$+2
_D_6060	LABEL	DWORD	;6060 shapes of intro1
	ORG	$+4
_D_6064	LABEL	WORD	;6064 rain: 1 in n chances of a new drop every frame
	ORG	$+2
_D_6066	LABEL	WORD	;6066 how far the monitor's picture went out of the screen on the left
	ORG	$+2
_D_6068	LABEL	DWORD	;6068 shapes of intro2
	ORG	$+4
_D_606C	LABEL	WORD	;606C rain: the maximum number of drops - 1
	ORG	$+2
_D_606E	LABEL	WORD	;606E 1: the monitor showed noise: redraw its frame
	ORG	$+2
_D_6070	LABEL	WORD	;6070 rain: the number of drops
	ORG	$+2
_D_6072	LABEL	BYTE	;6072 a 320x56 work surface (the A/B buttons)
	ORG	$+14
_D_6080	LABEL	WORD	;6080 
	ORG	$+2
_D_6082	LABEL	WORD	;6082 where the text area is on the screen: x (in 8-pixel columns?), y
	ORG	$+2
_D_6084	LABEL	WORD	;6084 where the text area is on the screen: x (in 8-pixel columns?), y
	ORG	$+2
_D_6086	LABEL	BYTE	;6086 copy of D_5732 the text area is shown in
	ORG	$+14
_D_6094	LABEL	BYTE	;6094 DOS version: major, minor
	ORG	$+1
_D_6095	LABEL	BYTE	;6095 DOS version: major, minor
	ORG	$+1
_D_6096	LABEL	WORD	;6096 twinkle: palette index (a global: the twinkle runs from the timer)
	ORG	$+2
_D_6098	LABEL	BYTE	;6098 twinkle: a palette entry
	ORG	$+6
_D_609E	LABEL	BYTE	;609E twinkle: a palette entry
	ORG	$+6
_D_60A4	LABEL	WORD	;60A4 twinkle: star index
	ORG	$+2
_D_60A6	LABEL	WORD	;60A6 twinkle: colour of the star's pixel
	ORG	$+2
_D_60A8	LABEL	WORD	;60A8 twinkle: colour of a lit star
	ORG	$+2
_D_60AA	LABEL	DWORD	;60AA starpos.dat
	ORG	$+4
_D_60AE	LABEL	WORD	;60AE twinkle: first star of the group that twinkles
	ORG	$+2
_D_60B0	LABEL	WORD	;60B0 number of stars
	ORG	$+2
_D_60B2	LABEL	WORD	;60B2 twinkle: random number (a shift register)
	ORG	$+10
_D_60BC	LABEL	WORD	;60BC colour of each star
	ORG	$+512
_D_62BC	LABEL	WORD	;62BC x of each star
	ORG	$+512
_D_64BC	LABEL	WORD	;64BC y of each star
	ORG	$+512
_D_66BC	LABEL	WORD	;66BC the hands: x0,y0, x1,y1
	ORG	$+8
_D_66C4	LABEL	WORD	;66C4 where the moving shapes go
	ORG	$+16
_D_66D4	LABEL	WORD	;66D4 the moving shapes: x,y each
	ORG	$+16

_BSS	ENDS

END
