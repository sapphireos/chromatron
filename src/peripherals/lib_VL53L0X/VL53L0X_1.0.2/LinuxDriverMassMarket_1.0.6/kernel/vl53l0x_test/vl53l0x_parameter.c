

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "vl53l0x_def.h"

//******************************** IOCTL definitions
#define VL53L0X_IOCTL_INIT			_IO('p', 0x01)
#define VL53L0X_IOCTL_XTALKCALB		_IOW('p', 0x02, unsigned int)
#define VL53L0X_IOCTL_OFFCALB		_IOW('p', 0x03, unsigned int)
#define VL53L0X_IOCTL_STOP			_IO('p', 0x05)
#define VL53L0X_IOCTL_SETXTALK		_IOW('p', 0x06, unsigned int)
#define VL53L0X_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)
#define VL53L0X_IOCTL_GETDATAS		_IOR('p', 0x0b, VL53L0X_RangingMeasurementData_t)
#define VL53L0X_IOCTL_PARAMETER		_IOWR('p', 0x0d, struct stmvl53l0x_parameter)
typedef enum {
	OFFSET_PAR = 0,
	XTALKRATE_PAR = 1,
	XTALKENABLE_PAR = 2,
	GPIOFUNC_PAR = 3,
	LOWTHRESH_PAR = 4,
	HIGHTHRESH_PAR = 5,
	DEVICEMODE_PAR = 6,
	INTERMEASUREMENT_PAR = 7,
} parameter_name_e;
/*
 *  IOCTL parameter structs
 */
struct stmvl53l0x_parameter {
	uint32_t is_read; //1: Get 0: Set
	parameter_name_e name;
	int32_t value;
	int32_t value2;
	int32_t status;
};

static void help(void)
{
	fprintf(stderr,
		"Usage: vl53l0x_parameter Parameter [Value]\n"
		" Parameter as <0: OFFSET, 1: XTALKRATE, 2: XTAKEENABLE, 3: GPIOFUNC, \n"
		"				4: LOW_THRESHOLD, 5: HIGH_THRESHOLD,\n"
		"				6: DEVICE_MODE, 7: INTERMEASUREMENT_MS\n"
		"				8: REFERENCESPADS_PAR, 9: REFCALIBRATION_PAR\n"
		" Value is optional for writting value to requested Parameter\n"
		);
	exit(1);
}

int main(int argc, char *argv[])
{
	int fd;
	struct stmvl53l0x_parameter parameter;
	char *end;

	if (argc < 2) {
		help();
		exit(1);
	}

	fd = open("/dev/stmvl53l0x_ranging",O_RDWR );
	if (fd <= 0) {
		fprintf(stderr,"Error open stmvl53l0x_ranging device: %s\n", strerror(errno));
		return -1;
	}

	parameter.name = strtoul(argv[1], &end, 10);
	if (*end) {
		help();
		exit(1);
	}
	if (argc == 2) {
		parameter.is_read = 1;
		fprintf(stderr, "To get VL53L0 parameter index:0x%x\n",
						parameter.name);

	} else {
		parameter.is_read = 0;
		parameter.value = strtoul(argv[2],&end,10);
		if (*end) {
	   		help();
			exit(1);
		}
		fprintf(stderr, "To set VL53L0 parameter index:0x%x, as value:%d\n",
						parameter.name, parameter.value);				
	}
	// to access requested register
	ioctl(fd, VL53L0X_IOCTL_PARAMETER,&parameter);
	fprintf(stderr," VL53L0 parameter value:%d, error status:%d\n",
				parameter.value, parameter.status);
	
	//close(fd);
	return 0;
}


