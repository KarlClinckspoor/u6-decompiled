/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	music/sound module

	- PC speaker sound effects (MUS_PlaySfx) built on the OSI_sound/
	  OSI_playNote/OSI_playNoise/OSI_playWavedNote primitives.
	- ambient sounds: each turn the 4 nearest sound emitting objects
	  (clock, fountain, fire, water wheel, musician...) are registered
	  (MUS_AddAmbientSource, sorted by distance in MUS_AmbientType/MUS_AmbientDist) and played on
	  every animation tick by MUS_PlayAmbient (silenced by "time stop").
	- music: a sound card driver U6xxx.DRV is loaded (MUS_InitDriver) and called
	  through the far pointer MUS_Driver, whose offset selects the entry point:
	    0: init(port)  6: play(song data)  9: stop
	    0x0c: set channel/instrument(channel, a, b) (Roland, from MIDI.DAT)
	  songs are the compressed xxx.m files listed in MUS_SongFiles (1-based index).
	MusicAdapter is the music adapter selected at startup:
	0:none(speaker) 1:Adlib 2:CMS 3:Tandy 4:Roland 5:Covox 6:Innovation.
*/
#include <ctype.h>

#include "u6.h"

/*28BA*/unsigned char MUS_SongIndex = 0;/*position in the musician's melody*/
/*28BB*/unsigned char MUS_Song = 0;/*musician's melody (0/1)*/
/*28BC*/unsigned char MUS_Clock = 0;/*clocks ring counter*/
/*28BD*/unsigned char MUS_Bell = 0;/*bells ring counter*/
/*28BE*/int16 MUS_MaxAmbient = 1;/*max ambient sounds played per tick*/
/*28C0*/int16 MUS_MIndex = 0;/*song currently playing (1-based, 0:none)*/
/*scale frequencies (one octave, 12 notes)*/
/*28C2*/int16 MUS_NoteFreq[] = {0xC2C,0xDA9,0xF56,0x103F,0x1136,0x123C,0x1478,0x16FA,0x1857,0x19CA,0x1B53,0x1EAB};
/*musician's melodies: 1..12 = note index+1 in MUS_NoteFreq, 0 = rest, 0xFF = end*/
/*melody I*/
/*28DA*/unsigned char MUS_Melody1[] = {
	2,6, 6, 0, 0,2,7, 7, 0,0,6,11,8,11,9,6,8,7,
	2,6, 0, 0, 0,2,7, 0, 0,0,6,11,8, 7,8,6,0,0,
	0,7,10,10,11,7,8,10,11,7,6, 8,7, 6,5,3,2,
0xFF};
/*melody II*/
/*2910*/unsigned char MUS_Melody2[] = {
	3,4, 6,0,12,0,0,0,9,9,11,9,8,9,7,0,
	2,3, 4,0,11,0,0,0,8,8, 9,8,7,8,6,0,
	3,4, 6,0,12,0,9,0,9,0,11,9,8,9,7,7,
	8,9,11,0, 7,0,8,0,6,0, 9,0,0,0,0,0,
0xFF};
/*2951,28DA,2910*/unsigned char *MUS_Melodies[2] = {MUS_Melody1, MUS_Melody2};


/*frequency sweep from <freqStart> to <freqEnd> during <duration>
ticks, changing every <step> ticks*/
/*MUS_0008*/static MUS_Sweep(int16 freqStart, int16 freqEnd, int16 step, int16 duration) {
	int16 freqIncr, t;
	int16 freq, stepTmp;

	if(duration > 0) {
		stepTmp = step;
		freqIncr = (freqEnd - freqStart) * stepTmp;
		freqIncr /= duration;
		freq = freqStart;
		for(t = 0; t < duration; freq += freqIncr, t += step) {
			OSI_sound(freq);
			OSI_sDelay(1, step);
		}
	}
	OSI_nosound();
}

/*play a speaker sound effect <sfx>; <param> is a frequency or a
distance/intensity depending on the effect:
	0:silence 1:short note(param) [bump] 2:harp 3:harpsichord
	4:hit 5:missile(param) 6:? 7:hail storm?(param) 8:earthquake
	9:dragged under 10:struggle 11:acid 12:failure/theft 13:spell failed
	14:spell success 15:breaking glass 16:missile, variant(param)
	17:melee swing 18:lute 19:panpipes 20:xylophone 21:awakening in jail
	22:squeak; default: note(param)*/
/*MUS_0065*/MUS_PlaySfx(int16 sfx, int16 param/*bp06*/) {
	int16 i;
	int16 freq;

	if(!SoundFlag)
		return;
	switch(sfx) {
		case 0: OSI_nosound(); break;
		case 1: OSI_playNote(param, 10); break;
		case 4: OSI_playNoise(1, 800, 10000); break;
		case 5:
			param = 2000 - (param << 4);
			for(i = 0; i < 40; i++) {
				if(OSI_rand(0, 3) == 0) {
					freq = OSI_rand(0, 200) + param - 100;
					if(freq > 19)
						OSI_sound(freq);
				} else {
					OSI_nosound();
				}
				OSI_sDelay(8, 1);
			}
			OSI_nosound();
		break;
		case 16:
			param = 1000 - (param << 3);
			for(i = 0; i < 40; i++) {
				if(OSI_rand(0, 1) && param > 20) {
					OSI_sound(OSI_rand(19, OSI_rand(20, param)));
				} else {
					OSI_nosound();
				}
				OSI_sDelay(8, 1);
			}
			OSI_nosound();
		break;
		case 6:
			param = 1000 - (param << 3);
			for(i = 0; i < 40; i++) {
				if(OSI_rand(0, 7) == 0) {
					freq = OSI_rand(0, param << 1) + param;
					if(freq > 19)
						OSI_sound(freq);
				} else {
					OSI_nosound();
				}
				OSI_sDelay(8, 1);
			}
			OSI_nosound();
		break;
		case 7:
			if(param < 0) {
				OSI_playNoise(1, 800, 3000);
				return;
			}
			for(i = 0; i < 40; i++) {
				if(param > 19) {
					if(OSI_rand(0, 7) == 0) {
						OSI_sound(OSI_rand(0, 40) + param);
					} else {
						OSI_nosound();
					}
				}
				OSI_sDelay(8, 1);
			}
			OSI_nosound();
		break;
		case 8:
			for(i = 0; i < 40; i++) {
				OSI_sound(OSI_rand(19, 200));
				OSI_sDelay(8, 1);
			}
			OSI_nosound();
		break;
		case 9: MUS_Sweep(1200, 2000, 1, 40); break;
		case 10: OSI_playNoise(1, 7000, 600); break;
		case 11:
			for(i = 0; i < 20; i++)
				OSI_playNoise(1, 350, OSI_rand(600, 4800));
		break;
		case 12:
		case 13: MUS_Sweep(800, 2000, 1, 50); break;
		case 14: OSI_playNote(2000, 10); break;
		case 15:
			for(i = 2000; i < 20000; i += 1000)
				OSI_playNoise(40, 120, i);
		break;
		case 17: MUS_Sweep(400, 750, 5, 150); break;
		case 20:
			OSI_playNote(param, 6);
			OSI_playWavedNote(param, 1, 700, 30000, -25);
		break;
		case 2: OSI_playWavedNote(param >> 1, 1, 6000, 30000, -5); break;
		case 3: OSI_playWavedNote(param, 1, 4000, 15000, -3); break;
		case 18: OSI_playWavedNote(param >> 1, 1, 2000, 20000, -20); break;
		case 19: OSI_playNote(param >> 2, 200); break;
		case 21:
			for(i = 0; i < 50; i++)
				OSI_playWavedNote((i << 4) + 800, 1, 1000, 10000 - (i << 6), (50 - i) << 2);
			OSI_playWavedNote(1600, 1, 8000, 0, 8);
		break;
		case 22: MUS_Sweep(5000, 8000, 1, 100); break;
		default: OSI_playNote(param, 100);
	}
}

/*register an ambient sound source at <distance> from the party:
keeps the 4 nearest sound emitting objects, sorted by distance*/
/*MUS_043B*/MUS_AddAmbientSource(int16 typeObject, int16 distance) {
	int16 pos;
	int16 i;

	pos = 0;
	if(
		typeObject != OBJ_CLOCK &&
		typeObject != OBJ_FOUNTAIN &&
		typeObject != OBJ_BELL &&
		typeObject != OBJ_FIREPLACE &&
		typeObject != OBJ_STALAGMITE &&
		typeObject != OBJ_FORGE_FIRE &&
		typeObject != OBJ_WATER_WHEEL &&
		typeObject != OBJ_LOG_SAW &&
		typeObject != OBJ_MILLSTONE &&
		typeObject != OBJ_COOK_FIRE &&
		typeObject != OBJ_FIRE_FIELD &&
		typeObject != OBJ_PROTECTION_FIELD &&
		typeObject != OBJ_VOLCANO &&
		typeObject != OBJ_MUSICIAN_SHORT_CAPE
	) {
		return;
	}
	/*insertion point*/
	for(; MUS_AmbientDist[pos] <= distance && pos < MUS_AmbientCount; pos ++);
	if(pos >= 4)
		return;
	/*shift the farther ones (the 4th is dropped)*/
	for(i = MUS_AmbientCount; i > pos; i --) {
		if(i < 4) {
			MUS_AmbientType[i] = MUS_AmbientType[i - 1];
			MUS_AmbientDist[i] = MUS_AmbientDist[i - 1];
		}
	}
	if(MUS_AmbientCount < 4)
		MUS_AmbientCount ++;
	MUS_AmbientType[pos] = typeObject;
	MUS_AmbientDist[pos] = distance;
}

/*clear the ambient sound sources list*/
/*MUS_051E*/MUS_ClearAmbient() {
	MUS_AmbientCount = 0;
}

/*sound sequencer: play the registered ambient sounds for one tick
(nearest first, at most MUS_MaxAmbient of them; nothing while time is stopped),
then update the clock/bell ringing counters*/
/*MUS_0525*/MUS_PlayAmbient() {
	int16 numSounds, dist;
	int16 i, j;

	numSounds = 0;
	if(SpellFx[14] == 0) {/*not in "time stop"*/
		for(i = numSounds; i < MUS_AmbientCount; i ++) {
			dist = MUS_AmbientDist[i];
			switch(MUS_AmbientType[i]) {
				case OBJ_CLOCK:/*clock*/
					if(MUS_Clock && (AnimFrameCounter & 3) == 0) {/*ringing the hour*/
						OSI_playWavedNote(0xc2c, 1, 2000, 20000, -10);
						numSounds ++;
					} else if((AnimFrameCounter & 0xf) == 0) {/*tic*/
						OSI_playNote(3000, 3 - (dist >> 2));
						numSounds ++;
					} else if((AnimFrameCounter & 0xf) == 8) {/*tac*/
						OSI_playNote(2000, 3 - (dist >> 2));
						numSounds ++;
					}
				break;
				case OBJ_BELL:/*bell*/
					if(MUS_Bell && (AnimFrameCounter & 0xf) == 0) {
						OSI_playWavedNote(0xc2c, 1, 2000, 20000, -10);
						numSounds ++;
					}
				break;
				/*Fireplace, Fire (from forge), Cook Fire, Fire Field*/
				case OBJ_FIREPLACE: case OBJ_FORGE_FIRE: case OBJ_COOK_FIRE: case OBJ_FIRE_FIELD:
					if(OSI_rand(0, 3) == 0) {
						numSounds ++;
						for(j = 0; j < 5; j ++) {
							if(OSI_rand(0, dist + 7) == 0)
								OSI_sound(OSI_rand(2000, 15000));
							else
								OSI_nosound();
							OSI_sDelay(8, 1);
						}
						OSI_nosound();
					}
				break;
				case OBJ_PROTECTION_FIELD:/*protection field*/
					if(OSI_rand(0, 1) == 0) {
						numSounds ++;
						for(j = 0; j < 80; j ++) {
							if(OSI_rand(0, 15) == 0)
								OSI_sound(OSI_rand(200, 1500 - (dist << 8)));
							else
								OSI_nosound();
							OSI_sDelay(8, 1);
						}
						OSI_nosound();
					}
				break;
				case OBJ_FOUNTAIN:/*fountain*/
					OSI_playNoise(10, 30 - (dist << 2), 25000 - (dist << 11));
					numSounds ++;
				break;
				case OBJ_WATER_WHEEL:/*water wheel*/
					OSI_playNoise(20, 60 - (dist << 2), 10000 - (dist << 10));
					numSounds ++;
				break;
				case OBJ_MUSICIAN_SHORT_CAPE:/*musician (short cape)*/
					/*one note of the melody (only without music card)*/
					if(MUS_Melodies[MUS_Song][MUS_SongIndex] && MusicAdapter == 0) {
						MUS_PlaySfx(18, MUS_NoteFreq[MUS_Melodies[MUS_Song][MUS_SongIndex] - 1]);
						numSounds ++;
					}
					if(MUS_Melodies[MUS_Song][++MUS_SongIndex] == 0xff)
						MUS_SongIndex = 0;
				break;
			}
			if(numSounds >= MUS_MaxAmbient)
				break;
		}
	}
	if(MUS_Clock && (AnimFrameCounter & 3) == 0)
		MUS_Clock --;
	if(MUS_Bell && (AnimFrameCounter & 0xf) == 0) {
		if(--MUS_Bell == 0)
			SetTileAnimation(BaseTile[OBJ_BELL], 0);
	}
	OSI_delay(1);
}

/*MIDI.DAT parser (data in TalkBuf at MUS_MidiDatPos): returns the next
decimal number, or -1 when the closing ')' is reached first*/
/*MUS_07F6*/static MUS_ParseMidiNumber() {
	int16 ch, value;

	while(1) {
		ch = TalkBuf[MUS_MidiDatPos];
		if(ch == ')')
			return -1;
		if(isdigit(ch))
			break;
		MUS_MidiDatPos ++;
	}
	value = 0;
	while(1) {
		ch = TalkBuf[MUS_MidiDatPos];
		if(!isdigit(ch))
			break;
		value = value * 10;
		value += ch - '0';
		MUS_MidiDatPos ++;
	}
	return value;
}

/*set up the (Roland) driver's channels for song <songIdx> (0-based):
MIDI.DAT holds one "( a b a b ...)" group per song, each pair (a, b)
being sent to driver entry 0x0c for channels 0, 1, ...;
without MIDI.DAT, all 16 channels get (2, 0)*/
/*MUS_0851*/static MUS_SetupMidiChannels(int16 songIdx) {
	int16 fd, ch;
	int16 parenCount, channel, val1, val2;

	DISK_confirm(DISK_INTRO);
	fd = OSI_open(/*2979*/"midi.dat");
	if(fd >= 0) {/*parse midi.dat*/
		OSI_read(fd, 0, 0x400, TalkBuf);
		parenCount = 0;
		for(MUS_MidiDatPos = 0; MUS_MidiDatPos < 0x400; MUS_MidiDatPos ++) {
			if(parenCount > songIdx) {
				for(channel = 0; (val1 = MUS_ParseMidiNumber()) >= 0; channel ++) {
					val2 = MUS_ParseMidiNumber();
					MUS_DRV(0xc)(channel, val1, val2);
				}
				break;
			}
			if(TalkBuf[MUS_MidiDatPos] == '(')
				parenCount ++;
		}
		OSI_close(fd);
	} else {/*no midi.dat*/
		for(ch = 0; ch < 16; ch++) {
			MUS_DRV(0xc)(ch, 2, 0);
		}
	}
}

/*songs (index 1..12 for MUS_PlaySong)*/
/*2955*/char *MUS_SongFiles[] = {
	/*2982*/"ultima.m",
	/*298B*/"bootup.m",
	/*2994*/"intro.m",
	/*299C*/"create.m",
	/*29A5*/"forest.m",
	/*29AE*/"hornpipe.m",
	/*29B9*/"engage.m",
	/*29C2*/"stones.m",
	/*29CB*/"dungeon.m",
	/*29D5*/"brit.m",
	/*29DC*/"gargoyle.m",
	/*29E7*/"end.m"
};

/*play a song: <song> is 1-based in MUS_SongFiles, 0 stops the music.
Does nothing without a music card, or if the song is already playing.
The song is loaded into TalkBuf.*/
/*MUS_091A*/MUS_PlaySong(int16 song/*bp06*/) {
	if(MusicAdapter == 0)
		return;
	if(song == MUS_MIndex)
		return;
	if(MUS_MIndex != 0) {
		MUS_DRV(9)();/*stop*/
		MUS_MIndex = 0;
	}
	if(song == 0)
		return;
	if(!SoundFlag)
		return;
	DISK_confirm(DISK_INTRO);
	if(MusicAdapter == 4)/*Roland*/
		MUS_SetupMidiChannels(song - 1);
	IsZFile = 1;
	LoadFile(MUS_SongFiles[song - 1], TalkBuf);
	IsZFile = 0;
	MUS_DRV(6)(TalkBuf);/*play*/
	MUS_MIndex = song;
}

/*play an "adequat" song for the current location/situation:
gargoyle land, random overland song, dungeon; hornpipe at sea;
engage in combat*/
/*MUS_09A8*/MUS_PlayAreaSong() {
	int16 song, objType;

	if(!SoundFlag)
		return;
	if(MapZ == 5) {
		song = 11;/*"gargoyle.m"*/
	} else if(MapZ == 0) {
		switch(OSI_rand(0, 3)) {
			case 0: song = 8; /*"stones.m"*/break;
			case 1: song = 1; /*"ultima.m"*/break;
			case 2: song = 5; /*"forest.m"*/break;
			case 3: song = 10;/*"brit.m"*/break;
		}
	} else {
		song = 9;/*"dungeon.m"*/
	}
	objType = GetType(Party[Active]);
	if(objType == OBJ_SHIP || objType == OBJ_SKIFF)
		song = 6;/*"hornpipe.m"*/
	if(InCombat)
		song = 7;/*"engage.m"*/
	MUS_PlaySong(song);
}

/*music drivers (index 1..6 = MusicAdapter)*/
/*296D*/char *MUS_DriverFiles[] = {
	/*29ED*/"U6ADLIB.DRV",
	/*29F9*/"U6CMS.DRV",
	/*2A03*/"U6TMUS.DRV",
	/*2A0E*/"U6ROLAND.DRV",
	/*2A1B*/"U6COVOX.DRV",
	/*2A27*/"U6INNOVA.DRV"
};

/*init music: load the driver for adapter <adapter> (MusicAdapter, 0:none)
and call its init entry with the i/o <port>*/
/*MUS_0A3A*/MUS_InitDriver(int16 adapter, int16 port) {
	if(adapter == 0)
		return;
	/*-- load music driver*/
	IsZFile = 1;
	MUS_Driver.p = LoadFile(MUS_DriverFiles[adapter - 1], 0L);
	IsZFile = 0;
	MUS_DRV(0)(port);
}
