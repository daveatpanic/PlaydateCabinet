//
//  main.c
//  MirrorJr
//
//  Created by Dave Hayden on 9/25/24.
//

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <netinet/in.h>

#include "SDL.h"

#include "frame.h"
#include "audio.h"
#include "stream.h"
#include "serial.h"
#include "controls.h"

bool checkExit()
{
	SDL_Event event;
	
	if ( SDL_PollEvent(&event) )
	{
		if ( event.type == SDL_QUIT )
			return true;
//		else if ( event.type != SDL_WINDOWEVENT )
//			printf("unhandled event type 0x%x\n", event.type);
	}
	
	return false;
}

void* copySerialToRingbuffer(void* ud);

int bytesread = 0;
time_t starttime;
bool serial_running = false;

/*
static void droproot()
{
	const char* username = "dave";
	struct passwd *pw = getpwnam(username);
	
	if (initgroups(pw->pw_name, pw->pw_gid) != 0 ||
	   setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
			fprintf(stderr, "droproot: Couldn't change to '%.32s' uid=%lu gid=%lu: %s\n",
				username, 
				(unsigned long)pw->pw_uid,
				(unsigned long)pw->pw_gid,
				strerror(errno));
			exit(1);
	}
}
*/

int get_ip_address(char *ip_buffer);

int main(int argc, const char * argv[])
{
	if ( SDL_InitSubSystem(SDL_INIT_VIDEO) != 0 )
	{
		printf("video init failed: %s", SDL_GetError());
		return false;
	}

/*
	int numModes = SDL_GetNumDisplayModes(0);
	for ( int i = 0; i < numModes; ++i )
	{
		SDL_DisplayMode mode;
		
		if ( SDL_GetDisplayMode(0, i, &mode) == 0 )
			printf("mode %i: fmt=%x w=%i h=%i refresh=%i\n", i, mode.format, mode.w, mode.h, mode.refresh_rate);
		else
			printf("Couldn't get display mode %i\n", i);
	}
*/
	
#if TARGET_MACOS
	SDL_Window* window = SDL_CreateWindow("Playdate", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1200, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
#else
	SDL_Window* window = SDL_CreateWindow("Playdate", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
#endif
	
	if ( window == NULL )
	{
		printf("couldn't create window: %s\n", SDL_GetError());
		return -1;
	}

	SDL_ShowCursor(SDL_DISABLE);
	
	if ( !controls_init() )
	{
		printf("error initializing control i/o\n");
		return -1;
	}
	
	if ( !stream_init() )
	{
		printf("error initializing stream input\n");
		return -1;
	}
	
	if ( !frame_init(window) )
		return -1;
	
	audio_init();
	//droproot();
	
	uint8_t ipaddr[INET_ADDRSTRLEN] = "....";
	get_ip_address((char*)ipaddr);
	frame_showWaitScreen(ipaddr);

	for ( ;; )
	{
		printf("waiting for playdate..\n");
	
		frame_showWaitScreen(ipaddr);

		printf("checking serial port\n");

		while ( !ser_isOpen() )
		{
//			printf("checking exit signal\n");

			if ( checkExit() )
				return 0;
			
			usleep(10000);

//			printf("calling ser_open()\n");
			ser_open();
		}
		
		printf("connected!\n");
		stream_begin();
	
		pthread_attr_t attrs;
		pthread_attr_init(&attrs);
	
//		struct sched_param sched = {.sched_priority=50};
//		pthread_attr_setschedparam(&attrs, &sched);
		
		serial_running = true;
		
		pthread_t readthread;
		pthread_create(&readthread, &attrs, copySerialToRingbuffer, NULL);
		
		pthread_attr_destroy(&attrs);
		
		time_t lastpoke = 0;
		starttime = time(NULL);
		
		while ( ser_isOpen() )
		{
			if ( checkExit() )
				return 0;
			
			time_t now = time(NULL);
			
			if ( now > lastpoke )
			{
				stream_poke();
				lastpoke = now;
				
				if ( now > starttime )
					; //printf("%i bytes read, %f kB/s\n", bytesread, (float)bytesread/(now-starttime)/1024.0f);
			}
			
			stream_process();
			controls_scan();
			usleep(1000);
		}
		
		audio_stop();
		stream_reset();
		
		serial_running = false;
		pthread_join(readthread, NULL);
	}
	
	return 0;
}

void* copySerialToRingbuffer(void* ud)
{
	printf("serial monitor started..\n");

	//FILE* log = fopen("log.bin", "w");
		
	while ( serial_running )
	{
		static uint8_t buf[65536];
		ssize_t n = ser_read(buf, sizeof(buf));
		
		if ( n > 0 )
		{
//			fwrite(buf, 1, (size_t)n, log);
//			fflush(log);

			bytesread += n;
			stream_addData(buf, (unsigned)n);
		}
		else if ( n < 0 )
		{
			printf("ser_read returned %i errno=%i\n", n, errno);
			break;
		}
	}
	
	printf("serial monitor ended..\n");
	return NULL;
}

#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>

int get_ip_address(char *ip_buffer)
{
	int fd;
	struct ifreq ifr;
	
	// Create a socket to retrieve information about the interface
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		perror("Socket creation failed");
		return -1;
	}

	strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ-1);
	ifr.ifr_name[IFNAMSIZ-1] = '\0';

	// Perform an ioctl call to get the IP address of the interface
	if (ioctl(fd, SIOCGIFADDR, &ifr) == -1) {
		perror("ioctl failed");
		close(fd);
		return -1;
	}

	// Convert the IP address to a string and store it in ip_buffer
	struct sockaddr_in *ip_addr = (struct sockaddr_in *)&ifr.ifr_addr;
	strncpy(ip_buffer, inet_ntoa(ip_addr->sin_addr), INET_ADDRSTRLEN);

	// Clean up and return success
	close(fd);
	return 0;
}
