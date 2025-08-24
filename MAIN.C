#include <stdio.h>
#include <conio.h>
#include <bios.h>

#include "cybervga.h"
#include "cv_rndr.h"
#include "cv_io.h"

int running=1;

int main()
{
	// Variables
	Byte far* VGA=(Byte far*)MK_FP(0xA000, 0);
	Byte far* screenBuffer;
	//

	// Init memory consumers :)
	cv_init_trig_tables();

	// Load assets

	// Double buffered screen setup
	screenBuffer = farmalloc(CV_SCREENRES);
	if(!screenBuffer) { printf("Not enough screen memory! Exiting...");goto CV_QUIT; }
	cv_set_vga_mode();

	// Main loop
	while(running)
	{
		// Clear screen
		_fmemset((Byte far*)screenBuffer,0,CV_SCREENRES);

		// IO FOR INPUT HANDLING
		if(cv_io_key_pressed())
		{
			unsigned char scanCode = cv_io_poll();

			switch(scanCode)
			{
				case KEY_ESC:
					running=0;
				break;

				default:
				break;
			}
		}

		// Game logic here

		// Render screen

		// Update main screen buffer
		_fmemcpy(VGA, screenBuffer, CV_SCREENRES);
	}


	// Finalize
	CV_QUIT:
	getch();
	cv_set_text_mode();
	farfree(screenBuffer);

	return 0;
}