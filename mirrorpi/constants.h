
#define VENDOR_NAME "Panic"
#define APP_NAME "mirror" // file names, keys, etc.
#define APP_DISPLAY "Mirror" // user display, window titles

#define FRAME_WIDTH      400
#define FRAME_HEIGHT     240
#define ROW_SIZE_BYTES   (FRAME_WIDTH/8)
#define FRAME_SIZE_BYTES (ROW_SIZE_BYTES * FRAME_HEIGHT)

#define NUM_BUTTONS 8

#define BLACK_R 0x31
#define BLACK_G 0x2f
#define BLACK_B 0x28

#define WHITE_R 0xb1
#define WHITE_G 0xaf
#define WHITE_B 0xa8

#define YELLOW_R 0xfb
#define YELLOW_G 0xc6
#define YELLOW_B 0x51

#define COLOR_TRIPLE(color) color##_R, color##_G, color##_B

enum {
	TIMER_StreamProtocolEnable = 1,
	TIMER_StreamProtocolStart = 2,
	TIMER_StreamProtocolPoke = 3,
	TIMER_GameControllerHandlerPoll = 4,
	TIMER_InputHandlerSendUpdate = 5,
};
