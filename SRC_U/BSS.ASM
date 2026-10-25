; Ultima VI - The False Prophet: U.EXE
; the uninitialised data (_BSS) of the C modules, at the addresses U.EXE has them.
; Turbo C 2.0 orders a module's _BSS by its symbol table, not by the order of
; definition, so (as SRC/BSS.ASM does for GAME.EXE) it is laid out here; the C modules
; declare these extern. Each label is followed by the address (DS offset) and what it is.

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

_D_4A14	dw 16 dup(?)	;4A14 the 5 main menu highlight colors (r, g, b)
_D_4A34	db 12 dup(?)	;4A34 
_D_4A40	dw 770 dup(?)	;4A40 VGA palette (r, g, b)
_D_5044	dw ?	;5044 video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA
_D_5046	db ?	;5046 mouse buttons
_D_5047	db ?	;5047 mouse buttons
_D_5048	dw ?	;5048 mouse x, y
_D_504A	dw 2 dup(?)	;504A mouse x, y
_D_504E	dd ?	;504E mouse pointer shapes
_D_5052	dw ?	;5052 mouse pointer shape
_D_5054	db 2 dup(?)	;5054 the surface the mouse pointer is drawn on
_D_5056	db 8 dup(?)	;5056 mouse background save area
_D_505E	db ?	;505E a mouse is present?
_D_505F	dw 768 dup(?)	;505F saved VGA palette
_D_565F	db 14 dup(?)	;565F the vellum's area
_D_566D	db 2 dup(?)	;566D the current text window
_D_566F	dw ?	;566F text cursor x, y
_D_5671	dw ?	;5671 text cursor x, y
_D_5673	dd ?	;5673 console: font
_D_5677	db ?	;5677 text colors
_D_5678	db ?	;5678 text colors
_D_5679	db 4 dup(?)	;5679 console: buffer
_D_567D	db 21 dup(?)	;567D the console: alignment (0 left, 1 right, 2 centred)
_D_5692	dd ?	;5692 the music driver's song
_D_5696	db 80 dup(?)	;5696 GAME.EXE's command line argument
_D_56E6	db 28 dup(?)	;56E6 
_D_5702	db 14 dup(?)	;5702 copy of D_565F
_D_5710	db 14 dup(?)	;5710 the screen
_D_571E	dw ?	;571E parse position in midi.dat
_D_5720	dw ?	;5720 music card i/o port
_D_5722	db 16 dup(?)	;5722 
_D_5732	db 14 dup(?)	;5732 the back buffer
_D_5740	db 6 dup(?)	;5740 free far memory
_D_5746	dw ?	;5746 music on
_D_5748	db 18 dup(?)	;5748 EGA: the page the noise is drawn into
_D_575A	dw ?	;575A set when the player gave up
_D_575C	dw 200 dup(?)	;575C scanline offsets
_D_58EC	dw ?	;58EC mouse on
_D_58EE	dw ?	;58EE graphic adapter: 0 VGA, 1 EGA, 2 Tandy, 3 CGA, 4 Hercules
_D_58F0	dw ?	;58F0 1: Escape was pressed: leave the introduction
_D_58F2	db 2 dup(?)	;58F2 blocks.xxx
_D_58F4	dw ?	;58F4 
_D_58F6	dd ?	;58F6 mouse pointer shapes (cursors.xxx)
_D_58FA	db 4 dup(?)	;58FA the music driver
_D_58FE	db 70 dup(?)	;58FE the buttons
_D_5944	db 16 dup(?)	;5944 name
_D_5954	dw ?	;5954 strength, dexterity, intelligence
_D_5956	dw ?	;5956 strength, dexterity, intelligence
_D_5958	dw ?	;5958 strength, dexterity, intelligence
_D_595A	db 8 dup(?)	;595A "Male"/"Female"
_D_5962	db 16 dup(?)	;5962 class
_D_5972	db ?	;5972 'G'
_D_5973	db ?	;5973 1: an Avatar
_D_5974	dw ?	;5974 experience
_D_5976	dw ?	;5976 level
_D_5978	dw ?	;5978 hit points, maximum
_D_597A	dw ?	;597A hit points, maximum
_D_597C	dw 2 dup(?)	;597C magic points
_D_5980	db 28 dup(?)	;5980 
_D_599C	db 14 dup(?)	;599C the text window of the name question
_D_59AA	db 14 dup(?)	;59AA the title
_D_59B8	dw 52 dup(?)	;59B8 the vellum has been shown
_D_5A20	db 14 dup(?)	;5A20 the character sheet
_D_5A2E	dw ?	;5A2E portrait (0-5)
_D_5A30	db 16 dup(?)	;5A30 name
_D_5A40	dw ?	;5A40 strength, dexterity, intelligence
_D_5A42	dw ?	;5A42 strength, dexterity, intelligence
_D_5A44	dw ?	;5A44 strength, dexterity, intelligence
_D_5A46	db 8 dup(?)	;5A46 sex: 'M' or 'F'
_D_5A4E	db 16 dup(?)	;5A4E "Avatar"
_D_5A5E	db ?	;5A5E 'G'
_D_5A5F	db ?	;5A5F 
_D_5A60	dw ?	;5A60 experience
_D_5A62	dw ?	;5A62 level
_D_5A64	dw ?	;5A64 hit points
_D_5A66	dw ?	;5A66 hit points, maximum
_D_5A68	dw ?	;5A68 magic points
_D_5A6A	db 104 dup(?)	;5A6A portrait
_D_5AD2	dd ?	;5AD2 montage image
_D_5AD6	dd ?	;5AD6 vellum image
_D_5ADA	db 2 dup(?)	;5ADA the save file read
_D_5ADC	db 10 dup(?)	;5ADC woods / gypsy image
_D_5AE6	db 20 dup(?)	;5AE6 the area of the gypsy's pattern
_D_5AFA	db 14 dup(?)	;5AFA EGA: the second page (segment a800)
_D_5B08	db 14 dup(?)	;5B08 EGA: the screen, for the mouse pointer
_D_5B16	dw ?	;5B16 
_D_5B18	dw 8 dup(?)	;5B18 level of each vial
_D_5B28	dw ?	;5B28 number of colors in the pattern
_D_5B2A	dw 8 dup(?)	;5B2A virtues
_D_5B3A	dw 32 dup(?)	;5B3A colors of the pattern
_D_5B7A	db 32 dup(?)	;5B7A pattern
_D_5B9A	dw ?	;5B9A script: position of the loop
_D_5B9C	dw ?	;5B9C script: loop count
_D_5B9E	dw ?	;5B9E script: the picture chosen by command 0x87
_D_5BA0	dw ?	;5BA0 palette entry
_D_5BA2	dw 3 dup(?)	;5BA2 palette colors
_D_5BA8	dw 3 dup(?)	;5BA8 palette colors
_D_5BAE	db 14 dup(?)	;5BAE twinkle: the surface the stars are drawn on
_D_5BBC	db 14 dup(?)	;5BBC 
_D_5BCA	db 14 dup(?)	;5BCA 
_D_5BD8	db 14 dup(?)	;5BD8 EGA: the second page
_D_5BE6	dw ?	;5BE6 clouds: their number
_D_5BE8	dw ?	;5BE8 script: the current one
_D_5BEA	dd ?	;5BEA clouds: the shapes
_D_5BEE	db 2 dup(?)	;5BEE clouds: the surface
_D_5BF0	db 2 dup(?)	;5BF0 rain: the surface
_D_5BF2	dw ?	;5BF2 rain: frames between the moves of a drop
_D_5BF4	dw ?	;5BF4 clouds: x, y of the big cloud bank
_D_5BF6	dw ?	;5BF6 clouds: x, y of the big cloud bank
_D_5BF8	dw ?	;5BF8 script: position in it
_D_5BFA	dw 150 dup(?)	;5BFA rain: x, y of each drop
_D_5D26	dw 10 dup(?)	;5D26 clouds: x, y, shape of each
_D_5D3A	dw 150 dup(?)	;5D3A rain: x, y of each drop
_D_5E66	dw 10 dup(?)	;5E66 clouds: x, y, shape of each
_D_5E7A	dw 10 dup(?)	;5E7A clouds: x, y, shape of each
_D_5E8E	dw 150 dup(?)	;5E8E rain: frames before the next move of each drop
_D_5FBA	db 14 dup(?)	;5FBA the monitor's screen
_D_5FC8	db 150 dup(?)	;5FC8 rain: shape of each drop
_D_605E	dw ?	;605E clouds: speed
_D_6060	dd ?	;6060 shapes of intro1
_D_6064	dw ?	;6064 rain: 1 in n chances of a new drop every frame
_D_6066	dw ?	;6066 how far the monitor's picture went out of the screen on the left
_D_6068	dd ?	;6068 shapes of intro2
_D_606C	dw ?	;606C rain: the maximum number of drops - 1
_D_606E	dw ?	;606E 1: the monitor showed noise: redraw its frame
_D_6070	dw ?	;6070 rain: the number of drops
_D_6072	db 14 dup(?)	;6072 a 320x56 work surface (the A/B buttons)
_D_6080	dw ?	;6080 
_D_6082	dw ?	;6082 where the text area is on the screen: x (in 8-pixel columns?), y
_D_6084	dw ?	;6084 where the text area is on the screen: x (in 8-pixel columns?), y
_D_6086	db 14 dup(?)	;6086 copy of D_5732 the text area is shown in
_D_6094	db ?	;6094 DOS version: major, minor
_D_6095	db ?	;6095 DOS version: major, minor
_D_6096	dw ?	;6096 twinkle: palette index (a global: the twinkle runs from the timer)
_D_6098	db 6 dup(?)	;6098 twinkle: a palette entry
_D_609E	db 6 dup(?)	;609E twinkle: a palette entry
_D_60A4	dw ?	;60A4 twinkle: star index
_D_60A6	dw ?	;60A6 twinkle: colour of the star's pixel
_D_60A8	dw ?	;60A8 twinkle: colour of a lit star
_D_60AA	dd ?	;60AA starpos.dat
_D_60AE	dw ?	;60AE twinkle: first star of the group that twinkles
_D_60B0	dw ?	;60B0 number of stars
_D_60B2	dw 5 dup(?)	;60B2 twinkle: random number (a shift register)
_D_60BC	dw 256 dup(?)	;60BC colour of each star
_D_62BC	dw 256 dup(?)	;62BC x of each star
_D_64BC	dw 256 dup(?)	;64BC y of each star
_D_66BC	dw 4 dup(?)	;66BC the hands: x0,y0, x1,y1
_D_66C4	dw 8 dup(?)	;66C4 where the moving shapes go
_D_66D4	dw 8 dup(?)	;66D4 the moving shapes: x,y each

_BSS	ENDS

END
