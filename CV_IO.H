#ifndef CV_IO_H
#define CV_IO_H

// Key definitions
#define KEY_LEFT 	0x4B
#define KEY_RIGHT 	0x4D
#define KEY_UP 		0x48
#define KEY_DOWN 	0x50
#define KEY_ESC 	0x01

// Init io
void cv_io_init(void);
// Return non zero if key pressed
int cv_io_key_pressed(void);
// Returns pressed key as scancode
unsigned char cv_io_poll(void);

#endif