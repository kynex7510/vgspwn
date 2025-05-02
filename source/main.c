#include "vgspwn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOME_DATA_START 0x32A000
#define HOME_DATA_SIZE 0x587F0

int main(int argc, char* argv[]) {
	gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, false);
	consoleInit(GFX_TOP, NULL);
	vgspwnInit();

	// Setup Buffers.
	void* linearBuffer = linearAlloc(HOME_DATA_SIZE);
	if (!linearBuffer) {
		printf("Linear buffer allocation failed!\n");
		goto end;
	}

	void* vramBuffer = vramAlloc(HOME_DATA_SIZE);
	if (!vramBuffer) {
		printf("VRAM buffer allocation failed!\n");
		goto end;
	}

	memset(linearBuffer, 0x41, HOME_DATA_SIZE);
	GSPGPU_FlushDataCache(linearBuffer, HOME_DATA_SIZE);

	GX_TextureCopy((u32*)linearBuffer, 0, (u32*)vramBuffer, 0, HOME_DATA_SIZE, 0x8);
	gspWaitForPPF();

	GSPGPU_InvalidateDataCache(vramBuffer, HOME_DATA_SIZE);

	linearFree(linearBuffer);

	// Do thing.

	vgspwnSelect(0);
	vgspwnAddTransfer((u32)vramBuffer, HOME_DATA_START, HOME_DATA_SIZE);
	vgspwnCommit();

	// Home menu should crash before we get here...

	vramFree(vramBuffer);
	printf("Something went wrong\n");

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
