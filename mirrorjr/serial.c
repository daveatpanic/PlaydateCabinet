//
//  serial.c
//  MirrorJr
//
//  Created by Dave Hayden on 9/28/24.
//

#include "serial.h"

#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>

#if TARGET_RPI
#include <libudev.h>
#endif

#if TARGET_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOBSD.h>
//#include <DiskArbitration/DiskArbitration.h>

#define kVendorID 0x1331 // Panic!
#define kPlaydateID 0x5740 // Playdate
#endif

//#include <gio/gio.h>

//#define LOG printf
#define LOG(...)

static int g_fd = -1;
//static const char* g_disk_label = NULL;

#if TARGET_RPI
static char* FindPlaydateSerialPort()
{
	struct udev* udev = udev_new();

	if ( !udev )
	{
		LOG("udev_new() failed\n");
		return NULL;
	}

	struct udev_enumerate* enumerate = udev_enumerate_new(udev);
	
	udev_enumerate_add_match_subsystem(enumerate, "tty");
	udev_enumerate_scan_devices(enumerate);

	struct udev_list_entry* list = udev_enumerate_get_list_entry(enumerate);
	struct udev_list_entry* node;
	char* result = NULL;

	udev_list_entry_foreach(node, list)
	{
		const char* path = udev_list_entry_get_name(node);
		
		if ( strncmp(path, "/sys/devices/platform", strlen("/sys/devices/platform")) != 0 )
			continue;

		if ( strncmp(path+strlen(path)-5, "ttyS0", 5) == 0 )
			continue;
		
		LOG("checking path %s\n", path);
		
		struct udev_device* dev = udev_device_new_from_syspath(udev, path);

		const char* vendor_id = udev_device_get_property_value(dev, "ID_USB_VENDOR_ID");
		const char* model_id = udev_device_get_property_value(dev, "ID_USB_MODEL_ID");
		const char* devname = udev_device_get_property_value(dev, "DEVNAME");

/*
		struct udev_list_entry *properties;
		properties = udev_device_get_properties_list_entry(dev);
		struct udev_list_entry *property;
		udev_list_entry_foreach(property, properties) {
			printf("Property: %s = %s\n",
				   udev_list_entry_get_name(property),
				   udev_list_entry_get_value(property));
		}
*/

		if ( vendor_id != NULL && model_id != NULL && strcmp(vendor_id, "1331") == 0 && strcmp(model_id, "5740") == 0 )
		{
			LOG("Found Playdate Device (%s): %s\n", devname, udev_device_get_property_value(dev, "ID_SERIAL_SHORT"));
			result = strdup(devname);
			udev_device_unref(dev);
			break;
		}
		
		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	return result;
}
#endif

#if TARGET_MACOS
bool CreateIOIteratorWithProductID(int product, io_iterator_t *iterator)
{
	CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBHostDeviceClassName);
	if ( matchingDict == NULL )
	{
		fprintf(stderr, "IOServiceMatching returned NULL.\n");
		return false;
	}

	int usbVendor = kVendorID;
	CFNumberRef refVendorId = CFNumberCreate (kCFAllocatorDefault, kCFNumberIntType, &usbVendor);
	CFDictionarySetValue (matchingDict, CFSTR (kUSBVendorID), refVendorId);
	CFRelease(refVendorId);

	int usbProduct = product;
	CFNumberRef refProductId = CFNumberCreate (kCFAllocatorDefault, kCFNumberIntType, &usbProduct);
	CFDictionarySetValue (matchingDict, CFSTR (kUSBProductID), refProductId);
	CFRelease(refProductId);

	io_iterator_t iter;
	kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iter);

	if ( kr == KERN_SUCCESS && iterator != NULL )
		*iterator = iter;

	return (kr == KERN_SUCCESS);
}

static char* FindPlaydateSerialPort()
{
	io_iterator_t iter = 0;

	if ( !CreateIOIteratorWithProductID(kPlaydateID, &iter) )
		return NULL;

	char* path = NULL;
	CFStringRef portPath = NULL;
	io_service_t device;

	while ( (device = IOIteratorNext(iter)) != 0 )
	{
		io_name_t deviceName;

		if ( IORegistryEntryGetName(device, deviceName) == KERN_SUCCESS && strcmp(deviceName, "Playdate") == 0 )
		{
			portPath = (CFStringRef)IORegistryEntrySearchCFProperty(device,
												   kIOServicePlane,
												   CFSTR (kIOCalloutDeviceKey),
												   kCFAllocatorDefault,
												   kIORegistryIterateRecursively);

			break;
		}
	}

	IOObjectRelease(iter);

	if ( portPath != NULL )
	{
		path = malloc(PATH_MAX);
		CFStringGetCString(portPath, path, PATH_MAX, kCFStringEncodingUTF8);
	}

	return path;
}
#endif

bool ser_open()
{
	char* dev = FindPlaydateSerialPort();
	
	if ( dev == NULL )
		return false;
	
	LOG("PlaydateSerialOpen (%s)\n", dev);

	struct flock lock, ourlock;

	if ( g_fd != -1 )
	{
		LOG("Serial port was already open (%d)\n", g_fd);
		free(dev);
		return true;
	}
		
	if ( access(dev, F_OK) == -1 )
	{
		LOG("Device %s does not exist\n", dev);
		free(dev);
		return false;
	}

	g_fd = open(dev, O_RDWR | /*O_NONBLOCK |*/ O_NOCTTY | O_SYNC | O_CLOEXEC );

	if ( g_fd == -1 )
	{
		LOG("Couldn't open %s (%d)\n", dev, errno);
		free(dev);
		return false;
	}

	free(dev);

	//lock port
	lock.l_type    = F_WRLCK;
	lock.l_start   = 0;
	lock.l_whence  = SEEK_SET;
	lock.l_len     = 0;
	ourlock = lock;

	fcntl(g_fd, F_GETLK, &lock);

	if ( lock.l_type == F_WRLCK || lock.l_type == F_RDLCK )
	{
		LOG("Serial port is in use.\n");
		ser_close();
		return false;
	}
	else
		fcntl(g_fd, F_SETLK, &ourlock);

	struct termios tty;
	memset(&tty, 0, sizeof tty);

	if ( tcgetattr(g_fd, &tty) == 0 )
	{
		speed_t speed = 115200;

		cfsetospeed(&tty, speed);
		cfsetispeed(&tty, speed);

		tty.c_cflag = (CLOCAL | CREAD); /* ignore modem controls  */
		tty.c_cflag &= (unsigned)~CSIZE;
		tty.c_cflag |= CS8;      /* 8-bit characters */
		tty.c_cflag &= (unsigned)~PARENB;  /* no parity bit */
		tty.c_cflag &= (unsigned)~CSTOPB;  /* only need 1 stop bit */
		tty.c_cflag &= (unsigned)~CRTSCTS; /* no hardware flowcontrol */

		/* setup for non-canonical mode */

		tty.c_iflag &= (unsigned)~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
//		tty.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
		tty.c_lflag &= (unsigned)~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
//		tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
		tty.c_oflag &= (unsigned)~OPOST;

		/* fetch bytes as they become available */

		// MIN == 0; TIME > 0: TIME specifies the limit for a timer in tenths of a second.
		// The timer is started when read(2) is called.
		// read(2) returns either when at least one byte of data is available, or when the timer expires.
		// If the timer expires without any input becoming available, read(2) returns 0.

		tty.c_cc[VMIN] = 0;
		tty.c_cc[VTIME] = 1;

		if ( tcsetattr(g_fd, TCSANOW, &tty) != 0 )
			LOG("tcsetattr failed (%d)\n", errno);
	}
	else
		LOG("tcgetattr failed (%d)\n", errno);

	return g_fd != -1;
}

bool ser_isOpen()
{
	return g_fd != -1;
}

void ser_close()
{
#if DEBUG
	LOG("PlaydateSerialClose (%d)", g_fd);
#endif
	if (g_fd != -1)
	{
		struct flock unlock;
		unlock.l_type    = F_UNLCK;
		unlock.l_start   = 0;
		unlock.l_whence  = SEEK_SET;
		unlock.l_len     = 0;
		fcntl(g_fd, F_SETLK, &unlock);

		close(g_fd);
		g_fd = -1;
	}
	else
		printf("Serial port was not open.\n");
}

static char ser_line_buffer[1024];
ssize_t ser_writeLine(const char* cmd)
{
	//LOG("PlaydateSerialWriteLine (%s)", wxString(cmd));

	ssize_t len_written = 0;
	int cmd_len = snprintf(ser_line_buffer, sizeof(ser_line_buffer), "%s\n", cmd);

	if ( cmd_len > 0 && ser_isOpen() )
		len_written = ser_write(ser_line_buffer, (unsigned)cmd_len);

	return len_written;
}

ssize_t ser_write(const char* buffer, size_t size)
{
	/*
	size_t loglen = size;
	
	while ( loglen > 0 && (buffer[loglen-1] == '\n' || buffer[loglen-1] == '\r') )
		--loglen;
	
	LOG("ser_write \"%.*s\"\n", loglen, buffer);
	*/
	
	ssize_t len_written = 0;

	if ( size > 0 )
	{
		const char* bufptr = buffer;

		while ( ser_isOpen() && size > 0 && len_written >= 0 )
		{
			len_written = write(g_fd, bufptr, size);
			
			if ( len_written > 0 )
			{
				size -= (size_t)len_written;
				bufptr += len_written;
			}
			else if ( len_written < 0 && errno == EAGAIN )
			{
				//try again
				len_written = 0;
			}
		}

		if ( size != 0 )
			LOG("Serial write Failed: (%zd != %zu)\n", len_written, size);
	}
	else
		LOG("Serial write invalid state.\n");

	return len_written;
}


ssize_t ser_writeNonblocking(const char* buffer, size_t size)
{
	/*
	size_t loglen = size;

	while ( loglen > 0 && (buffer[loglen-1] == '\n' || buffer[loglen-1] == '\r') )
		--loglen;

	LOG("ser_writeNonblocking \"%.*s\"\n", loglen, buffer);
	*/
	
	//size_t len_written = 0;

	if ( size > 0 && ser_isOpen() )
	{
		ssize_t n = write(g_fd, buffer, size);
		
		if ( n == -1 )
			ser_close();
		else
			return n;
	}

	return -1;
}

ssize_t ser_read(uint8_t* buffer, size_t size)
{
	if ( !ser_isOpen() )
		return -1;
	
	ssize_t n = read(g_fd, buffer, size);
	
	if ( n == -1 )
	{
		if ( errno == EAGAIN )
			return 0;
		else
			ser_close();
	}

	return n;
}

void ser_flush(void)
{
	tcflush(g_fd, TCIOFLUSH);
}
