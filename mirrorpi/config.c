
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

#define CONFIG_FILE "cabinet.cnf"

struct config default_config = { .up = 4, .down = 27, .left = 22, .right = 23, .b = 24, .a = 25, .menu = 5, .dock = -1, .rotary1 = -1, .rotary2 = -1, .rotarymult = 3 };

void get_config(struct config* config)
{
	*config = default_config;

	FILE* f = fopen(CONFIG_FILE, "r");
	
	if ( f != NULL )
	{
		char buf[128];
		#define match(s, c) if ( strncmp(buf, s, strlen(s)) == 0 ) config->(c) = atoi(&buf[strlen(s)]);
		
		while ( fgets(buf, sizeof(buf), f) )
		{
			if ( strncmp(buf, "up ", strlen("up ")) == 0 )
			 	config->up = atoi(&buf[strlen("up ")]);
			else if ( strncmp(buf, "down ", strlen("down ")) == 0 )
				config->down = atoi(&buf[strlen("down ")]);
			else if ( strncmp(buf, "left ", strlen("left ")) == 0 )
				config->left = atoi(&buf[strlen("left ")]);
			else if ( strncmp(buf, "right ", strlen("right ")) == 0 )
				config->right = atoi(&buf[strlen("right ")]);
			else if ( strncmp(buf, "b ", strlen("b ")) == 0 )
				config->b = atoi(&buf[strlen("b ")]);
			else if ( strncmp(buf, "a ", strlen("a ")) == 0 )
				config->a = atoi(&buf[strlen("a ")]);
			else if ( strncmp(buf, "menu ", strlen("menu ")) == 0 )
				config->menu = atoi(&buf[strlen("menu ")]);
			else if ( strncmp(buf, "dock ", strlen("dock ")) == 0 )
				config->dock = atoi(&buf[strlen("dock ")]);
			else if ( strncmp(buf, "rotary1 ", strlen("rotary1 ")) == 0 )
				config->rotary1 = atoi(&buf[strlen("rotary1 ")]);
			else if ( strncmp(buf, "rotary2 ", strlen("rotary2 ")) == 0 )
				config->rotary2 = atoi(&buf[strlen("rotary2 ")]);
			else if ( strncmp(buf, "rotarymult ", strlen("rotarymult ")) == 0 )
				config->rotarymult = atoi(&buf[strlen("rotarymult ")]);
		}
		
		fclose(f);
	}
}
