PUBLIC Mem_BP,Mem_Remain
PUBLIC Names,DEXTE
PUBLIC Mouse
PUBLIC Leader,INTEL,Party
PUBLIC NPCMode,ObjShapeType
PUBLIC GrMemory,NPCFlag,TalkBuf,ScreenScanlines,Amount
PUBLIC HitPoints,AnimTile,BaseTile,ObjPos
PUBLIC Schedule,VGAPalette,OrigShapeType
PUBLIC Screen
PUBLIC PathData,TileFlag,STREN,MovePts,GrMemorySeg,Level
PUBLIC PartySize,AreaTiles,SchedIndex,AnimFrameMask,AreaFlags,MAGIC
PUBLIC ExpPoints,AnimFirstFrame,NPCComMode,ScratchBuf
PUBLIC MapSurface
PUBLIC MapScanlines,NPCStatus,ObjStatus
PUBLIC AreaLight,TalkFlags,TerrainType,TileFlag2,FarHeap,AnimFrameShift,TypeWeight,FarHeapSize,NumAnimData
PUBLIC SchedPointer
PUBLIC PointerShapes
PUBLIC HexInputBuf,StateAnimData,PromptCnt,MouseHeldCmd,LineBufChars,LineBufColors
PUBLIC PointerX,PointerY,IncantScrX,IncantScrY
PUBLIC Selection
PUBLIC TextWindows
PUBLIC AdvanceDir,IncantLetters,SelectionIsDark,VarInt,LinesBeforeMore,MouseButtons,VarStr
PUBLIC MouseArea
PUBLIC ChunkCacheAge,ChunkIndexBuf,ChunkCacheIdx,ChunkCache,AreaX,AreaY
PUBLIC VisQueueX
PUBLIC VisQueueY,AreaOffset,SearchRect,FreeMonsterHead
PUBLIC SearchZ,Link,FreeObjHead,CellObjTile,TileStringBuf,Tile_11x11,MapObjPtr
PUBLIC Obj_11x11,CellObjNext,FreeObjCount,FreeMonsterCount,Equipment,WeightEquip,InvenSize
PUBLIC WeightInven,Party_2Handed,InvenOwner,NPC_2Handed,ShowNPCInven,InvenSlotObj,
PUBLIC TalkKeyword,TalkInput,TalkNpcName,TalkNpcs
PUBLIC TalkAnswered,TalkDescPC,TalkBase
PUBLIC TalkSavedPC,Talk_PC,TalkStrBuf
PUBLIC Spellbook,SpellList,SpellbookPageCount,SpellSavedStatus,SpellPageNav,SpellNextCircle
PUBLIC SpellbookFirst,SpellResult,SelectedSpell,SeanceFlag,PickpocketActive,SpellbookPage,SpellPageCounts
PUBLIC PTH_Cur,PTH_Top,PTH_Index,PTH_AnX,PTH_Found,PTH_AnY,PTH_MaxCost
PUBLIC PTH_direct,PTH_object,FormationObjs,PartyGravityX,PartyGravityY,PartyFlankPos,EnemiesGravityX
PUBLIC PartyFrontMember,EnemiesGravityY,EnemyFlankPos,LastActiveIdx,PartyFlankNeg,EnemyFrontFirst,EnemyFlankNeg
PUBLIC EnemiesNum
PUBLIC MissileEnd,TripleBoltX,TripleBoltY,EquipWeapons,MissileHitList,MissileHitObj
PUBLIC MUS_MidiDatPos,MUS_AmbientType,MUS_AmbientCount,MUS_AmbientDist,MUS_Driver
PUBLIC GrDriver,DirtyRect,GrInfo,DosVerMajor,DosVerMinor


_BSS	SEGMENT PUBLIC WORD 'BSS'

Mem_BP	dd ?	;D_322E
Mem_Remain	dd ?	;D_3232
Names	db 16 * 14 dup(?)	;D_3236
DEXTE	db 100h dup(?)	;D_3316
Mouse	db 19h dup(?)	;struct tMouse	;D_3416
Leader	dd ?	;D_342F
INTEL	db 100h dup(?)	;D_3433
Party	db 11h dup(?)	;D_3533
NPCMode	dd ?	;D_3544
ObjShapeType	dw 0C00h dup(?)	;D_3548
GrMemory	dd ?	;D_4D48
NPCFlag	dd ?	;D_4D4C
TalkBuf	dd ?	;D_4D50
ScreenScanlines	dw 200 dup(?)	;D_4D54 scanlines offset
Amount	db 0C00h dup(?,?)	;D_4EE4
HitPoints	db 100h dup(?)	;D_66E4
AnimTile	dw 20h dup(?)	;D_67E4 part of animdata	;D_67E4
BaseTile	dd ?	;D_6824
ObjPos	db 3*0C00h dup(?)	;D_6828
Schedule	dd ?	;D_8C28
VGAPalette	dd ?	;D_8C2C u6pal
OrigShapeType	dd ?	;D_8C30
Screen	db 0Eh dup(?)	;struct tSurface	;D_8C34
PathData	dd ?	;D_8C42 [savegame\\objlist]
TileFlag	dd ?	;D_8C46
STREN	db 100h dup(?)	;D_8C4A
MovePts	db 100h dup(?)	;D_8D4A
GrMemorySeg	dw ?	;D_8E4A
Level	dd ?	;D_8E4C
PartySize	db ?	;D_8E50
AreaTiles	db 40*40 dup(?)	;D_8E51
SchedIndex	dd ?	;D_9491
AnimFrameMask	db 20h dup(?)	;D_9495 part of animdata
AreaFlags	db 40*40 dup(?)	;D_94B5
MAGIC	db 100h dup(?)	;D_9AF5
ExpPoints	dw 100h dup(?)	;D_9BF5
AnimFirstFrame	dw 20h dup(?)	;D_9DF5 part of animdata
NPCComMode	dd ?	;D_9E35
ScratchBuf	dd ?	;D_9E39
;-- struct tSurface --
MapSurface	dw ?	;D_9E3D D_9E3D.segment
__9E3F	dd ?	;D_9E3D.pScanlines
__9E43	dw ?	;D_9E3D.right
__9E45	dw ?	;D_9E3D.left
__9E47	dw ?	;D_9E3D.top
__9E49	dw ?	;D_9E3D.bottom
;-- --
MapScanlines	dw 176 dup(?)	;D_9E4B scanlines offset
NPCStatus	db 100h dup(?)	;D_9FAB
ObjStatus	db 0C00h dup(?)	;D_A0AB
AreaLight	db 40*40 dup(?)	;D_ACAB
TalkFlags	db 100h dup(?)	;D_B2EB
TerrainType	dd ?	;part of tileflag	;D_B3EB
TileFlag2	dd ?	;D_B3EF part of tileflag
FarHeap	dd ?	;D_B3F3 all system far memory
AnimFrameShift	db 20h dup(?)	;D_B3F7 part of animdata
TypeWeight	dd ?	;part of tileflag	;D_B417
FarHeapSize	dd ?	;D_B41B farcoreleft result
NumAnimData	dw ?	;part of animdata	;D_B41F
SchedPointer	dw (100h + 1) dup(?)	;D_B421
PointerShapes	dd ?	;D_B623
__B627	db ?
;--
HexInputBuf	db 5 dup(?)	;D_B628 a string?
StateAnimData	db 21h dup(?)	;D_B62D
PromptCnt	db ?	;D_B64E
MouseHeldCmd	dw ?	;D_B64F
LineBufChars	db 29h dup(?)	;D_B651
LineBufColors	db 29h dup(?)	;D_B67A
PointerX	dw ?	;D_B6A3
PointerY	dw ?	;D_B6A5
IncantScrX	db 4 dup(?)	;D_B6A7
IncantScrY	db 4 dup(?)	;D_B6AB
Selection	dw ?	;D_B6AF
	dw ?	;D_B6B1
	dw ?	;D_B6B3
TextWindows	db 9 dup(?)	;D_B6B5 window/clip info?
__B6BE	db 9 dup(?)	;window/clip info?
__B6C7	db 9 dup(?)
__B6D0	db 9 dup(?)	;window/clip info?
AdvanceDir	dw ?	;D_B6D9
IncantLetters	db 4 dup(?)	;D_B6DB
SelectionIsDark	dw ?	;D_B6DF
VarInt	dw (10+26) dup(?)	;D_B6E1
LinesBeforeMore	dw ?	;D_B729
MouseButtons	dw ?	;D_B72B
VarStr	dw (10+26) dup(?)	;D_B72D
;-- struct tSurface	;D_B775
MouseArea	dw ?	;MouseArea.segment
__B777	dd ?	;MouseArea.pScanlines
__B77B	dw ?	;MouseArea.right
__B77D	dw ?	;MouseArea.left
__B77F	dw ?	;MouseArea.top
__B781	dw ?	;MouseArea.bottom
;--
__B783	db ?	;padding?
;--
ChunkCacheAge	dw 10h dup(?)	;D_B784
ChunkIndexBuf	dd ?	;D_B7A4 part of map
ChunkCacheIdx	dw 10h dup(?)	;D_B7A8
ChunkCache	db 10h * 8 * 8 dup(?)	;D_B7C8
AreaX	dw ?	;D_BBC8
AreaY	dw ?	;D_BBCA
VisQueueX	db 100h dup(?)	;D_BBCC
VisQueueY	db 100h dup(?)	;D_BCCC
AreaOffset	dw ?	;D_BDCC
SearchRect	dw 4 dup(?)	;D_BDCE
FreeMonsterHead	dw ?	;D_BDD6
SearchZ	dw ?	;D_BDD8
Link	dw 0C00h dup(?)	;D_BDDA
FreeObjHead	dw ?	;D_D5DA
CellObjTile	dw 100h dup(?)	;D_D5DC
TileStringBuf	db 19h dup(?)	;D_D7DC
Tile_11x11	dw 11*11 dup(?)	;D_D7F5
MapObjPtr	dw 40*40 dup(?)	;D_D8E7
Obj_11x11	db 11*11 dup(?)	;D_E567
CellObjNext	db 100h dup(?)	;D_E5E0
FreeObjCount	dw ?	;D_E6E0
FreeMonsterCount	dw ?	;D_E6E2
Equipment	dw 8 dup(?)	;D_E6E4
WeightEquip	dw ?	;D_E6F4
InvenSize	db ?	;D_E6F6
WeightInven	dw ?	;D_E6F7
Party_2Handed	db 10h dup(?)	;D_E6F9
InvenOwner	dw ?	;D_E709
NPC_2Handed	dw ?	;D_E70B
ShowNPCInven	dw ?	;D_E70D
InvenSlotObj	dw 0Ch dup(?)	;D_E70F
__E727	db ?
;--
TalkKeyword	db 0Ah dup(?)	;D_E728
TalkInput	db 32h dup(?)	;D_E732 input string for conversation?
TalkNpcName	db 32h dup(?)	;D_E764 interlocutor's name
TalkNpcs	dw 2 dup(?)	;D_E796 interlocutor/locutor
__E79A	db ?
__E79B	db ?
TalkAnswered	db ?	;D_E79C
TalkDescPC	dd ?	;D_E79D
TalkBase	dw ?	;D_E7A1
__E7A3	db 4 dup(?)
TalkSavedPC	dd ?	;D_E7A7
Talk_PC	dw ?	;D_E7AB
TalkStrBuf	db 10*100 dup(?)	;D_E7AD
__EB95	db ?
Spellbook	dw 2 dup(?)	;D_EB96
SpellList	dw 8 dup(?)	;D_EB9A
SpellbookPageCount	db ?	;D_EBAA
SpellSavedStatus	dw ?	;D_EBAB
SpellPageNav	db 3 dup(?)	;D_EBAD
SpellNextCircle	db ?	;D_EBB0
SpellbookFirst	db ?	;D_EBB1
SpellResult	db ?	;D_EBB2
SelectedSpell	db ?	;D_EBB3
SeanceFlag	db ?	;D_EBB4
PickpocketActive	db ?	;D_EBB5
SpellbookPage	db 8 dup(?)	;D_EBB6
SpellPageCounts	db 8 dup(?)	;D_EBBE
PTH_Cur	db ?	;D_EBC6
PTH_Top	dw ?	;D_EBC7
PTH_Index	db ?	;D_EBC9
PTH_AnX	dw ?	;D_EBCA
PTH_Found	dw ?	;D_EBCC
PTH_AnY	dw ?	;D_EBCE
PTH_MaxCost	db ?	;D_EBD0
PTH_direct	dw ?	;D_EBD1
PTH_object	dw ?	;D_EBD3
FormationObjs	db 10h dup(?)	;D_EBD5
PartyGravityX	dw ?	;D_EBE5
PartyGravityY	dw ?	;D_EBE7
PartyFlankPos	dw ?	;D_EBE9
EnemiesGravityX	dw ?	;D_EBEB
PartyFrontMember	dw ?	;D_EBED
EnemiesGravityY	dw ?	;D_EBEF
EnemyFlankPos	dw ?	;D_EBF1
LastActiveIdx	dw ?	;D_EBF3
PartyFlankNeg	dw ?	;D_EBF5
EnemyFrontFirst	dw ?	;D_EBF7
EnemyFlankNeg	dw ?	;D_EBF9
EnemiesNum	dw ?	;D_EBFB
__EBFD	db ?
MissileEnd	dw 2 dup(?)	;D_EBFE missile x,y?
TripleBoltX	dw 3 dup(?)	;D_EC02
TripleBoltY	dw 3 dup(?)	;D_EC08
EquipWeapons	dw 8 dup(?)	;D_EC0E
MissileHitList	dw 40h dup(?)	;D_EC1E
MissileHitObj	dw ?	;D_EC9E
MUS_MidiDatPos	dw ?	;D_ECA0 index in MIDI.DAT?
MUS_AmbientType	dw 4 dup(?)	;D_ECA2
MUS_AmbientCount	dw ?	;D_ECAA
MUS_AmbientDist	dw 4 dup(?)	;D_ECAC
MUS_Driver	dw ?,?	;D_ECB4 music driver
;-- --
GrDriver	dw ?,?	;D_ECB8 graphic driver
DirtyRect	dw 4 dup(?)	;D_ECBC
GrInfo	dd ?	;D_ECC4
DosVerMajor	db ?	;D_ECC8 dos version.major
DosVerMinor	db ?	;D_ECC9 dos version.minor

_BSS	ENDS

END
