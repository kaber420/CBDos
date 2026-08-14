#include <stdio.h>

#include "m_argv.h"

#include "doomgeneric.h"

#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);


void doomgeneric_Create(int argc, char **argv)
{
	// save arguments
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();

#ifdef ARDUINO
	// ESP32: allocate screen buffer in PSRAM (320*200*4 = 256KB)
	DG_ScreenBuffer = (pixel_t*)heap_caps_malloc(
	    DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t),
	    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!DG_ScreenBuffer) {
	    printf("[DOOM] FATAL: cannot allocate screen buffer in PSRAM!\n");
	    return;
	}
#else
	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
#endif

	DG_Init();

	D_DoomMain ();
}

