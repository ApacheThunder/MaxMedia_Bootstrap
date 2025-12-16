/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <nds/fifocommon.h>
#include <nds/fifomessages.h>
#include <nds/arm9/input.h>
#include <fat.h>
#include <sys/stat.h>

#include "args.h"
#include "file_browse.h"
#include "font.h"
#include "hbmenu_consolebg.h"

#include "iconTitle.h"
#include "nds_loader_arm9.h"
#include "dldi_tools.h"
#include "dldi_binaries.h"
#include "defaultFrame.h"
#include "tonccpy.h"
#include "skin.h"
#include "lzss.h"

#define BG_256_COLOR	(BIT(7))
#define FlashBase_S98	0x09000000

using namespace std;

ALIGN(16) volatile u8* FrameBuffer;

DTCM_DATA int err = 0;
DTCM_DATA u32 DecompressedFrameSize = 0x024036;
DTCM_DATA bool GUIINIT = false;
DTCM_DATA bool gbaGuiEnabled = false;
DTCM_DATA bool slot1Available = false;
DTCM_DATA bool slot2Available = false;
DTCM_DATA bool usingSlot2 = false;

DTCM_DATA int PathCount = 2;

DTCM_DATA ALIGN(16) const char* DSiAutoBootPath = "fat:/_picoboot.nds";

DTCM_DATA ALIGN(16) const char* AutoBootSlot1Paths[] = {
	"fat:/bootme.nds",
	"fat:/boot.nds"
};

DTCM_DATA ALIGN(16) const char* AutoBootSlot2Paths[] = {
	"slot2:/bootme.nds",
	"slot2:/boot.nds"
};



u16 Read_S98NOR_ID() {
	*((vu16*)(FlashBase_S98)) = 0xF0;	
	*((vu16*)(FlashBase_S98+0x555*2)) = 0xAA;
	*((vu16*)(FlashBase_S98+0x2AA*2)) = 0x55;
	*((vu16*)(FlashBase_S98+0x555*2)) = 0x90;
	return *((vu16*)(FlashBase_S98+0xE*2));
}

void SetKernelRomPage() {
	*(vu16*)0x09FE0000 = 0xD200;
	*(vu16*)0x08000000 = 0x1500;
	*(vu16*)0x08020000 = 0xD200;
	*(vu16*)0x08040000 = 0x1500;
	*(vu16*)0x09880000 = 0x8002; // Kernel section of NorFlash
	*(vu16*)0x09FC0000 = 0x1500;
}


void InitGUIForGBA() {
	if (gbaGuiEnabled)return;
	gbaGuiEnabled = true;
	GUIINIT = false;
	videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
	vramSetBankC(VRAM_C_SUB_BG_0x06200000);
	vramSetBankD(VRAM_D_LCD);
	// for the main screen
	REG_BG3CNT = BG_BMP16_256x256 | BG_BMP_BASE(0) | BG_WRAP_OFF;
	REG_BG3PA = 1 << 8; //scale x
	REG_BG3PB = 0; //rotation x
	REG_BG3PC = 0; //rotation y
	REG_BG3PD = 1 << 8; //scale y
	REG_BG3X = 0; //translation x
	REG_BG3Y = 0; //translation y*/
	toncset((void*)BG_BMP_RAM(0),0,0x18000);
	toncset((void*)BG_BMP_RAM(8),0,0x18000);
	swiWaitForVBlank();
}

void InitGUI (void) {
	if (GUIINIT)return;
	GUIINIT = true;
	gbaGuiEnabled = false;
	iconTitleInit();
	videoSetModeSub(MODE_4_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	int bgSub = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 1, 0);
	PrintConsole *console = consoleInit(0, 0, BgType_Text4bpp, BgSize_T_256x256, 4, 6, false, false);
	dmaCopy(hbmenu_consolebgBitmap, bgGetGfxPtr(bgSub), 256*256);
	ConsoleFont font;
	font.gfx = (u16*)fontTiles;
	font.pal = (u16*)fontPal;
	font.numChars = 95;
	font.numColors = (fontPalLen / 2);
	font.bpp = 4;
	font.asciiOffset = 32;
	font.convertSingleColor = true;
	consoleSetFont(console, &font);
	dmaCopy(hbmenu_consolebgPal, BG_PALETTE_SUB, 256*2);
	BG_PALETTE_SUB[255] = RGB15(31,31,31);
	keysSetRepeat(25,5);
	consoleSetWindow(console, 1, 1, 30, 22);
}


void gbaMode() {
	InitGUIForGBA();
	sysSetCartOwner(true);
	
	swiWaitForVBlank();
	
	if (Read_S98NOR_ID() == 0x223D)SetKernelRomPage();
	
	FrameBuffer = (u8*)malloc(DecompressedFrameSize);
	
	LZ77_Decompress((u8*)defaultFrame, (u8*)FrameBuffer);
	
	LoadSkinFromBinary(3, (u8*)FrameBuffer, DecompressedFrameSize);
	
	if(PersonalData->gbaScreen) { lcdMainOnBottom(); } else { lcdMainOnTop(); }
	
	sysSetCartOwner(false);
	fifoSendValue32(FIFO_USER_01, 1);
	REG_IME = 0;
	irqDisable(IRQ_VBLANK);
	while(1)swiWaitForVBlank();
}

int exitProgram(void) {
	if (!GUIINIT)InitGUI();
	if (err != 0)iprintf("Bootloader returned error %d\n", err);
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if (!keysHeld())break;
	}
	while (1) {
		swiWaitForVBlank();
		scanKeys();
		if (keysDown() != 0)break;
	}
	systemShutDown();
	return 0;
}

int DoFatError(void) {
	if (!GUIINIT) {  InitGUI(); } else { consoleClear(); }
	printf ("\n\n\n\n\n\n\n\n\n\n        FAT INIT FAILED!\n");
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if (!keysHeld())break;
	}
	while (1) {
		swiWaitForVBlank();
		scanKeys();
		if (keysDown() != 0)break;
	}
	systemShutDown();
	return 0;
}


void TrySlot1Init() {
	dldiLoadFromBin(gmtf_dldi);	
	slot1Available = fatMountSimple("fat", dldiGet());
}

void TrySlot2Init() {
	if (isDSiMode()) {
		slot2Available = false;
		return;
	}
	dldiLoadFromBin2(mmcf_dldi);
	if (fatMountSimple("slot2", dldiGet2())) {
		usingSlot2 = true;
		slot2Available = true;
		return;
	}
}


int FileBrowser() {
	InitGUI();
	consoleClear();
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if (!keysHeld())break;
	}
	
	if (!isDSiMode() && !slot2Available)TrySlot2Init();
		
	if (slot2Available) {
		if (access("slot2:/", F_OK) == 0)chdir("slot2:/");
	} else {
		if (access("fat:/", F_OK) == 0)chdir("fat:/");
	}
	
	vector<string> extensionList = argsGetExtensionList();
	
	while(1) {
		string filename = browseForFile(extensionList);
		// Construct a command line
		vector<string> argarray;
		if (!argsFillArray(filename, argarray)) {
			printf("Invalid NDS or arg file selected\n");
		} else {
			iprintf("Running %s with %d parameters\n", argarray[0].c_str(), argarray.size());
			// Make a copy of argarray using C strings, for the sake of runNdsFile
			vector<const char*> c_args;
			for (const auto& arg: argarray)c_args.push_back(arg.c_str());
			// Try to run the NDS file with the given arguments
			int err = runNdsFile(c_args[0], c_args.size(), &c_args[0]);
			iprintf("Start failed. Error %i\n", err);
		}
		argarray.clear();
	}
	return 0;
}


int main(void) {
	extern u64 *fake_heap_end;
	*fake_heap_end = 0;
	
	defaultExceptionHandler();
	
	sysSetCardOwner(BUS_OWNER_ARM9);
	sysSetCartOwner(BUS_OWNER_ARM9);
	
	mmcf_dldi[0] = 0xED;
	gmtf_dldi[0] = 0xED;
	
	TrySlot1Init();
	
	if (!slot1Available) {
		if (!isDSiMode())TrySlot2Init();
		if (!slot2Available) {
			if (isDSiMode()) {
				return DoFatError();
			} else {
				gbaMode();
				return 0;
			}
		}
	}

	scanKeys();
	swiWaitForVBlank();
	switch (keysCurrent()) {
		default: {
			err = FileBrowser();
			return exitProgram();
		} break;
		case KEY_B: {
			if (isDSiMode()) {
				err = FileBrowser();
				return exitProgram();
			} else {
				gbaMode();
				return 0;
			}
		} break;
		case 0: {
			const char* bootPath = 0;
			
			if (slot1Available) {
				if (isDSiMode()) {
					if (access(DSiAutoBootPath, F_OK) == 0)bootPath = DSiAutoBootPath;
				} else {
					for (int i = 0; i < PathCount; i++) {
						if (access(AutoBootSlot1Paths[i], F_OK) == 0) {
							bootPath = AutoBootSlot1Paths[i];
							break;
						}
					}
				}
			} else if (!isDSiMode()) {
				for (int i = 0; i < PathCount; i++) {
					if (access(AutoBootSlot2Paths[i], F_OK) == 0) {
						bootPath = AutoBootSlot2Paths[i];
						break;
					}
				}
			}
			if(bootPath != 0) {
				const char *argarray[1] = { bootPath };
				err = runNdsFile(bootPath, 1, argarray);
				return exitProgram();
			} else {
				err = FileBrowser();
				return exitProgram();
			}
		} break;
	}
}

