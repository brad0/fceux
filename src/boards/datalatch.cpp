/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Xodnizel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "mapinc.h"
#include "../ines.h"

static uint8 latche=0, latcheinit=0, bus_conflict=0;
static uint16 addrreg0=0, addrreg1=0;
static uint8 *WRAM = NULL;
static uint32 WRAMSIZE=0;
static void (*WSync)(void) = nullptr;
static uint8 submapper;

static DECLFW(LatchWrite) {
//	FCEU_printf("bs %04x %02x\n",A,V);
	if (bus_conflict)
		latche = V & CartBR(A);
	else
		latche = V;
	WSync();
}

static void LatchPower(void) {
	latche = latcheinit;
	WSync();
	if (WRAM) {
		SetReadHandler(0x6000, 0xFFFF, CartBR);
		SetWriteHandler(0x6000, 0x7FFF, CartBW);
		FCEU_CheatAddRAM(WRAMSIZE >> 10, 0x6000, WRAM);
	} else {
		SetReadHandler(0x8000, 0xFFFF, CartBR);
	}
	SetWriteHandler(addrreg0, addrreg1, LatchWrite);
}

static void LatchClose(void) {
	if (WRAM)
		FCEU_gfree(WRAM);
	WRAM = NULL;
}

static void StateRestore(int version) {
	WSync();
}

static void Latch_Init(CartInfo *info, void (*proc)(void), uint8 init, uint16 adr0, uint16 adr1, uint8 wram, uint8 busc) {
	bus_conflict = busc;
	latcheinit = init;
	addrreg0 = adr0;
	addrreg1 = adr1;
	WSync = proc;
	info->Power = LatchPower;
	info->Close = LatchClose;
	GameStateRestore = StateRestore;
	submapper = info->submapper;
	if(info->ines2)
		if(info->battery_wram_size + info->wram_size > 0)
			wram = 1;
	if (wram)
	{
		if(info->ines2)
		{
			//I would like to do it in this way, but FCEUX is woefully inadequate
			//for instance if WRAMSIZE is large, the cheat pointers may get overwritten. and it's just a giant mess.
			//WRAMSIZE = info->battery_wram_size + info->wram_size;
			//WRAM = (uint8*)FCEU_gmalloc(WRAMSIZE);
			//if(!info->wram_size && !info->battery_wram_size) {}
			//else if(info->wram_size && !info->battery_wram_size)
			//	SetupCartPRGMapping(0x10, WRAM, info->wram_size, 1);
			//else if(!info->wram_size && info->battery_wram_size)
			//{
			//	SetupCartPRGMapping(0x10, WRAM, info->battery_wram_size, 1);
			//	info->addSaveGameBuf( WRAM, info->battery_wram_size );
			//} else {
			//	//well, this is annoying
			//	SetupCartPRGMapping(0x10, WRAM, info->wram_size, 1);
			//	SetupCartPRGMapping(0x11, WRAM, info->battery_wram_size, 1); //? ? ? there probably isnt even a way to select this
			//	info->addSaveGameBuf( WRAM + info->wram_size, info->battery_wram_size );
			//}
			
			//this is more likely the only practical scenario
			WRAMSIZE = 8192;
			WRAM = (uint8*)FCEU_gmalloc(WRAMSIZE);
			SetupCartPRGMapping(0x10, WRAM, WRAMSIZE, 1);
			SetReadHandler(0x6000, 0x7FFF, CartBR);
			SetWriteHandler(0x6000, 0x7FFF, CartBW);
			setprg8r(0x10, 0x6000, 0);
			if(info->battery_wram_size)
			{
				info->addSaveGameBuf( WRAM, 8192 );
			}
		}
		else
		{
			WRAMSIZE = 8192;
			WRAM = (uint8*)FCEU_gmalloc(WRAMSIZE);
			SetupCartPRGMapping(0x10, WRAM, WRAMSIZE, 1);
			if (info->battery) {
				info->addSaveGameBuf( WRAM, WRAMSIZE );
			}
			AddExState(WRAM, WRAMSIZE, 0, "WRAM");
		}
		
	}
	AddExState(&latche, 1, 0, "LATC");
}

//------------------ Map 0 ---------------------------

#ifdef DEBUG_MAPPER
static DECLFW(NROMWrite) {
	FCEU_printf("bs %04x %02x\n", A, V);
	CartBW(A, V);
}
#endif

static void NROMPower(void) {
	setprg8r(0x10, 0x6000, 0);	// Famili BASIC (v3.0) need it (uses only 4KB), FP-BASIC uses 8KB
	setprg16(0x8000, ~1);
	setprg16(0xC000, ~0);
	setchr8(0);

	SetReadHandler(0x6000, 0x7FFF, CartBR);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);
	SetReadHandler(0x8000, 0xFFFF, CartBR);

	FCEU_CheatAddRAM(WRAMSIZE >> 10, 0x6000, WRAM);

#ifdef DEBUG_MAPPER
	SetWriteHandler(0x4020, 0xFFFF, NROMWrite);
	#endif
}

void NROM_Init(CartInfo *info) {
	info->Power = NROMPower;
	info->Close = LatchClose;

	WRAMSIZE = 8192;
	WRAM = (uint8*)FCEU_gmalloc(WRAMSIZE);
	SetupCartPRGMapping(0x10, WRAM, WRAMSIZE, 1);
	if (info->battery) {
		info->addSaveGameBuf( WRAM, WRAMSIZE );
	}
	AddExState(WRAM, WRAMSIZE, 0, "WRAM");
}

//------------------ Map 2 ---------------------------

static void UNROMSync(void) {
//	static uint32 mirror_in_use = 0;
//	if (PRGsize[0] <= 128 * 1024) {
//		setprg16(0x8000, latche & 0x7);
//		if (latche & 8) mirror_in_use = 1;
//		if (mirror_in_use)
//			setmirror(((latche >> 3) & 1) ^ 1);	// Higway Star Hacked mapper, disabled till new mapper defined
//	} else
	setprg16(0x8000, latche);
	setprg16(0xc000, ~0);
	setchr8(0);
}

void UNROM_Init(CartInfo *info) {
	Latch_Init(info, UNROMSync, 0, 0x8000, 0xFFFF, 0, info->ines2 && info->submapper == 2);
}

//------------------ Map 3 ---------------------------

static void CNROMSync(void) {
	setchr8(latche);
	setprg32(0x8000, 0);
	setprg8r(0x10, 0x6000, 0);	// Hayauchy IGO uses 2Kb or RAM
}

void CNROM_Init(CartInfo *info) {
	Latch_Init(info, CNROMSync, 0, 0x8000, 0xFFFF, 1, info->ines2 && info->submapper == 2);
}

//------------------ Map 7 ---------------------------

static void ANROMSync() {
	setprg32(0x8000, latche & 0xF);
	setmirror(MI_0 + ((latche >> 4) & 1));
	setchr8(0);
}

void ANROM_Init(CartInfo *info) {
	Latch_Init(info, ANROMSync, 0, 0x4020, 0xFFFF, 0, 0);
}

//------------------ Map 8 ---------------------------

static void M8Sync() {
	setprg16(0x8000, latche >> 3);
	setprg16(0xc000, 1);
	setchr8(latche & 3);
}

void Mapper8_Init(CartInfo *info) {
	Latch_Init(info, M8Sync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 11 ---------------------------

static void M11Sync(void) {
	setprg32(0x8000, latche & 0xF);
	setchr8(latche >> 4);
}

void Mapper11_Init(CartInfo *info) {
	Latch_Init(info, M11Sync, 0, 0x8000, 0xFFFF, 0, 0);
}

void Mapper144_Init(CartInfo *info) {
	Latch_Init(info, M11Sync, 0, 0x8001, 0xFFFF, 0, 0);
}

//------------------ Map 13 ---------------------------

static void CPROMSync(void) {
	setchr4(0x0000, 0);
	setchr4(0x1000, latche & 3);
	setprg32(0x8000, 0);
}

void CPROM_Init(CartInfo *info) {
	Latch_Init(info, CPROMSync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 29 ---------------------------	//Used by Glider, http://www.retrousb.com/product_info.php?cPath=30&products_id=58

static void M29Sync() {
	setprg16(0x8000, (latche & 0x1C) >> 2);
	setprg16(0xc000, ~0);
	setchr8r(0, latche & 3);
	setprg8r(0x10, 0x6000, 0);
}

void Mapper29_Init(CartInfo *info) {
	Latch_Init(info, M29Sync, 0, 0x8000, 0xFFFF, 1, 0);
}


//------------------ Map 38 ---------------------------

static void M38Sync(void) {
	setprg32(0x8000, latche & 3);
	setchr8(latche >> 2);
}

void Mapper38_Init(CartInfo *info) {
	Latch_Init(info, M38Sync, 0, 0x7000, 0x7FFF, 0, 0);
}

//------------------ Map 66 ---------------------------

static void MHROMSync(void) {
	setprg32(0x8000, latche >> 4);
	setchr8(latche & 0xF);
}

void MHROM_Init(CartInfo *info) {
	Latch_Init(info, MHROMSync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 70 ---------------------------

static void M70Sync() {
	setprg16(0x8000, latche >> 4);
	setprg16(0xc000, ~0);
	setchr8(latche & 0xf);
}

void Mapper70_Init(CartInfo *info) {
	Latch_Init(info, M70Sync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 78 ---------------------------
/* Should be two separate emulation functions for this "mapper".  Sigh.  URGE TO KILL RISING. */
static void M78Sync() {
	setprg16(0x8000, (latche & 7));
	setprg16(0xc000, ~0);
	setchr8(latche >> 4);
	if (submapper == 3) {
		setmirror((latche >> 3) & 1);	
	} else {
		setmirror(MI_0 + ((latche >> 3) & 1));
	}
}

void Mapper78_Init(CartInfo *info) {
	Latch_Init(info, M78Sync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 86 ---------------------------
// Moero!! Pro Yakyuu has an ADPCM chip with internal ROM,
// used for voice samples (not dumped, so emulation isn't possible)
static void M86Sync(void) {
	setprg32(0x8000, (latche >> 4) & 3);
	setchr8((latche & 3) | ((latche >> 4) & 4));
}

void Mapper86_Init(CartInfo *info) {
	Latch_Init(info, M86Sync, ~0, 0x6000, 0x6FFF, 0, 0);
}

//------------------ Map 87 ---------------------------

static void M87Sync(void) {
	setprg32(0x8000, 0);
	setchr8(((latche >> 1) & 1) | ((latche << 1) & 2));
}

void Mapper87_Init(CartInfo *info) {
	Latch_Init(info, M87Sync, ~0, 0x6000, 0xFFFF, 0, 0);
}

//------------------ Map 89 ---------------------------

static void M89Sync(void) {
	setprg16(0x8000, (latche >> 4) & 7);
	setprg16(0xc000, ~0);
	setchr8((latche & 7) | ((latche >> 4) & 8));
	setmirror(MI_0 + ((latche >> 3) & 1));
}

void Mapper89_Init(CartInfo *info) {
	Latch_Init(info, M89Sync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 93 ---------------------------

static void SSUNROMSync(void) {
	setprg16(0x8000, latche >> 4);
	setprg16(0xc000, ~0);
	setchr8(0);
}

void SUNSOFT_UNROM_Init(CartInfo *info) {
	Latch_Init(info, SSUNROMSync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 94 ---------------------------

static void M94Sync(void) {
	setprg16(0x8000, latche >> 2);
	setprg16(0xc000, ~0);
	setchr8(0);
}

void Mapper94_Init(CartInfo *info) {
	Latch_Init(info, M94Sync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 97 ---------------------------

static void M97Sync(void) {
	setprg16(0x8000, ~0);
	setprg16(0xc000, latche & 15);
	setmirror((latche >> 7) & 1);
	setchr8(0);
}

void Mapper97_Init(CartInfo *info) {
	Latch_Init(info, M97Sync, ~0, 0x8000, 0xBFFF, 0, 0);
}

//------------------ Map 107 ---------------------------

static void M107Sync(void) {
	setprg32(0x8000, (latche >> 1) & 3);
	setchr8(latche & 7);
}

void Mapper107_Init(CartInfo *info) {
	Latch_Init(info, M107Sync, ~0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 113 ---------------------------

static void M113Sync(void) {
	setprg32(0x8000, (latche >> 3) & 7);
	setchr8(((latche >> 3) & 8) | (latche & 7));
//	setmirror(latche>>7); // only for HES 6in1
}

void Mapper113_Init(CartInfo *info) {
	Latch_Init(info, M113Sync, 0, 0x4100, 0x7FFF, 0, 0);
}

//------------------ Map 140 ---------------------------

void Mapper140_Init(CartInfo *info) {
	Latch_Init(info, MHROMSync, 0, 0x6000, 0x7FFF, 0, 0);
}

//------------------ Map 152 ---------------------------

static void M152Sync() {
	setprg16(0x8000, (latche >> 4) & 7);
	setprg16(0xc000, ~0);
	setchr8(latche & 0xf);
	setmirror(MI_0 + ((latche >> 7) & 1));	/* Saint Seiya...hmm. */
}

void Mapper152_Init(CartInfo *info) {
	Latch_Init(info, M152Sync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 180 ---------------------------

static void M180Sync(void) {
	setprg16(0x8000, 0);
	setprg16(0xc000, latche);
	setchr8(0);
}

void Mapper180_Init(CartInfo *info) {
	Latch_Init(info, M180Sync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 184 ---------------------------

static void M184Sync(void) {
	setchr4(0x0000, latche);
	setchr4(0x1000, latche >> 4);
	setprg32(0x8000, 0);
}

void Mapper184_Init(CartInfo *info) {
	Latch_Init(info, M184Sync, 0, 0x6000, 0x7FFF, 0, 0);
}

//------------------ Map 203 ---------------------------

static void M203Sync(void) {
	setprg16(0x8000, (latche >> 2) & 3);
	setprg16(0xC000, (latche >> 2) & 3);
	setchr8(latche & 3);
}

void Mapper203_Init(CartInfo *info) {
	Latch_Init(info, M203Sync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ Map 218 ---------------------------

static void Mapper218_Power()
{
	//doesn't really matter
	SetReadHandler(0x6000, 0xFFFF, &CartBROB);
}

void Mapper218_Init(CartInfo *info)
{
	info->Power = &Mapper218_Power;

	//fixed PRG mapping
	setprg32(0x8000, 0);

	//this mapper is supposed to interpret the iNES header bits specially
	static const uint8 mirrorings[] = {MI_V,MI_H,MI_0,MI_1};
	SetupCartMirroring(mirrorings[info->mirrorAs2Bits],1,nullptr);

	//cryptic logic to effect the CHR RAM mappings by mapping 1k blocks to NTARAM according to how the pins are wired
	//this could be done by bit logic, but this is self-documenting
	static const uint8 mapping[] = {
		0,1,0,1,0,1,0,1, //mirrorAs2Bits==0
		0,0,1,1,0,0,1,1, //mirrorAs2Bits==1
		0,0,0,0,1,1,1,1, //mirrorAs2Bits==2
		0,0,0,0,0,0,0,0  //mirrorAs2Bits==3
	};
	for(int i=0;i<8;i++)
		VPageR[i] = &NTARAM[mapping[info->mirrorAs2Bits*8+i]];

	PPUCHRRAM = 0xFF;
}

//------------------ Map 240 ---------------------------

static void M240Sync(void) {
	setprg8r(0x10, 0x6000, 0);
	setprg32(0x8000, latche >> 4);
	setchr8(latche & 0xF);
}

void Mapper240_Init(CartInfo *info) {
	Latch_Init(info, M240Sync, 0, 0x4020, 0x5FFF, 1, 0);
}

//------------------ Map 241 ---------------------------
// Mapper 7 mostly, but with SRAM or maybe prot circuit
// figure out, which games do need 5xxx area reading

static void M241Sync(void) {
	setchr8(0);
	setprg8r(0x10, 0x6000, 0);
	if (latche & 0x80)
		setprg32(0x8000, latche | 8);	// no 241 actually, but why not afterall?
	else
		setprg32(0x8000, latche);
}

void Mapper241_Init(CartInfo *info) {
	Latch_Init(info, M241Sync, 0, 0x8000, 0xFFFF, 1, 0);
}

//------------------ A65AS ---------------------------

// actually, there is two cart in one... First have extra mirroring
// mode (one screen) and 32K bankswitching, second one have only
// 16 bankswitching mode and normal mirroring... But there is no any
// correlations between modes and they can be used in one mapper code.

static void BMCA65ASSync(void) {
	if (latche & 0x40)
		setprg32(0x8000, (latche >> 1) & 0x0F);
	else {
		setprg16(0x8000, ((latche & 0x30) >> 1) | (latche & 7));
		setprg16(0xC000, ((latche & 0x30) >> 1) | 7);
	}
	setchr8(0);
	if (latche & 0x80)
		setmirror(MI_0 + (((latche >> 5) & 1)));
	else
		setmirror(((latche >> 3) & 1) ^ 1);
}

void BMCA65AS_Init(CartInfo *info) {
	Latch_Init(info, BMCA65ASSync, 0, 0x8000, 0xFFFF, 0, 0);
}

//------------------ BMC-11160 ---------------------------
// Simple BMC discrete mapper by TXC

static void BMC11160Sync(void) {
	uint32 bank = (latche >> 4) & 7;
	setprg32(0x8000, bank);
	setchr8((bank << 2) | (latche & 3));
	setmirror((latche >> 7) & 1);
}

void BMC11160_Init(CartInfo *info) {
	Latch_Init(info, BMC11160Sync, 0, 0x8000, 0xFFFF, 0, 0);
}
