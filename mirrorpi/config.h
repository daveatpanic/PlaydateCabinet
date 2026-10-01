#include <stdint.h>

struct config
{
	int up;
	int down;
	int left;
	int right;
	int b;
	int a;
	int menu;
	
	int dock;
	int rotary1;
	int rotary2;
	
	int rotarymult;
};

void get_config(struct config* config);
