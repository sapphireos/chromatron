

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "vl53l0x_def.h"

#define MODE_RANGE		0
#define MODE_XTAKCALIB	1
#define MODE_OFFCALIB	2
#define MODE_HELP		3
#define MODE_PARAMETER  6


//******************************** IOCTL definitions
#define VL53L0X_IOCTL_INIT			_IO('p', 0x01)
#define VL53L0X_IOCTL_XTALKCALB		_IOW('p', 0x02, unsigned int)
#define VL53L0X_IOCTL_OFFCALB		_IOW('p', 0x03, unsigned int)
#define VL53L0X_IOCTL_STOP			_IO('p', 0x05)
#define VL53L0X_IOCTL_SETXTALK		_IOW('p', 0x06, unsigned int)
#define VL53L0X_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)
#define VL53L0X_IOCTL_GETDATAS		_IOR('p', 0x0b, VL53L0X_RangingMeasurementData_t)
#define VL53L0X_IOCTL_PARAMETER		_IOWR('p', 0x0d, struct stmvl53l0x_parameter)

//modify the following macro accoring to testing set up
#define OFFSET_TARGET		100//200
#define XTALK_TARGET		600//400
#define NUM_SAMPLES			20//20

typedef enum {
	OFFSET_PAR = 0,
	XTALKRATE_PAR = 1,
	XTALKENABLE_PAR = 2,
	GPIOFUNC_PAR = 3,
	LOWTHRESH_PAR = 4,
	HIGHTHRESH_PAR = 5,
	DEVICEMODE_PAR = 6,
	INTERMEASUREMENT_PAR = 7,
	REFERENCESPADS_PAR = 8,
	REFCALIBRATION_PAR = 9,
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
		"Usage: vl53l0x_test [-c] [-h]\n"
		" -h for usage\n"
		" -c for crosstalk calibration\n"
		" -o for offset calibration\n"
		" default for ranging\n"
		);
	exit(1);
}

int main(int argc, char *argv[])
{
	int fd;
	unsigned long data;
	VL53L0X_RangingMeasurementData_t range_datas;
	struct stmvl53l0x_parameter parameter;
	int flags = 0;
	int mode = MODE_RANGE;
	unsigned int targetDistance=0;
	int i = 0;

	/* handle (optional) flags first */
	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
		case 'c': mode= MODE_XTAKCALIB; break;
		case 'h': mode= MODE_HELP; break;
		case 'o': mode = MODE_OFFCALIB; break;
		default:
			fprintf(stderr, "Error: Unsupported option "
				"\"%s\"!\n", argv[1+flags]);
			help();
			exit(1);
		}
		flags++;
	}
	if (mode == MODE_HELP)
	{
		help();
		exit(0);
	}

	fd = open("/dev/stmvl53l0x_ranging",O_RDWR | O_SYNC);
	if (fd <= 0)
	{
		fprintf(stderr,"Error open stmvl53l0x_ranging device: %s\n", strerror(errno));
		return -1;
	}
	//make sure it's not started
	if (ioctl(fd, VL53L0X_IOCTL_STOP , NULL) < 0) {
		fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_STOP : %s\n", strerror(errno));
		close(fd);
		return -1;
	}	
	if (mode == MODE_XTAKCALIB)
	{
		unsigned int XtalkInt = 0;
		uint8_t XtalkEnable = 0;
		fprintf(stderr, "xtalk Calibrate place black target at %dmm from glass===\n",XTALK_TARGET);
		// to xtalk calibration 
		targetDistance = XTALK_TARGET;
		if (ioctl(fd, VL53L0X_IOCTL_XTALKCALB , &targetDistance) < 0) {
			fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_XTALKCALB : %s\n", strerror(errno));
			close(fd);
			return -1;
		}
		// to get xtalk parameter
		parameter.is_read = 1;
		parameter.name = XTALKRATE_PAR;
		if (ioctl(fd, VL53L0X_IOCTL_PARAMETER , &parameter) < 0) {
			fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER : %s\n", strerror(errno));
			close(fd);
			return -1;
		}
		XtalkInt = (unsigned int)parameter.value;
		parameter.name = XTALKENABLE_PAR;
		if (ioctl(fd, VL53L0X_IOCTL_PARAMETER , &parameter) < 0) {
			fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER : %s\n", strerror(errno));
			close(fd);
			return -1;
		}
		XtalkEnable = (uint8_t)parameter.value;
		fprintf(stderr, "VL53L0 Xtalk Calibration get Xtalk Compensation rate in fixed 16 point as %u, enable:%u\n",XtalkInt,XtalkEnable);
		
		for(i = 0; i <= NUM_SAMPLES; i++)
		{
			usleep(30 *1000); /*100ms*/
					// to get all range data
			ioctl(fd, VL53L0X_IOCTL_GETDATAS,&range_datas);	
		}
		fprintf(stderr," VL53L0 DMAX calibration Range Data:%d,  signalRate_mcps:%d\n",range_datas.RangeMilliMeter, range_datas.SignalRateRtnMegaCps);
		// get rangedata of last measurement to avoid incorrect datum from unempty buffer 
		//to close
		close(fd);
		return -1;
	}
	else if (mode == MODE_OFFCALIB)
	{
		int offset=0;
		uint32_t SpadCount=0;
		uint8_t IsApertureSpads=0;
		uint8_t VhvSettings=0,PhaseCal=0;


		// to xtalk calibration 
		targetDistance = OFFSET_TARGET;
		if (ioctl(fd, VL53L0X_IOCTL_OFFCALB , &targetDistance) < 0) {
			fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_OFFCALB : %s\n", strerror(errno));
			close(fd);
			return -1;
		}
		// to get current offset
		parameter.is_read = 1;
		parameter.name = OFFSET_PAR;
		if (ioctl(fd, VL53L0X_IOCTL_PARAMETER, &parameter) < 0) {
			fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER : %s\n", strerror(errno));
			close(fd);
			return -1;
		}
		offset = (int)parameter.value;
		fprintf(stderr, "get offset %d micrometer===\n",offset);
		
		parameter.name = REFCALIBRATION_PAR;
		if (ioctl(fd, VL53L0X_IOCTL_PARAMETER, &parameter) < 0) {
			fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER : %s\n", strerror(errno));
			close(fd);
			return -1;
		}
		VhvSettings = (uint8_t) parameter.value;
		PhaseCal=(uint8_t) parameter.value2;
		fprintf(stderr, "get VhvSettings is %u ===\nget PhaseCas is %u ===\n", VhvSettings,PhaseCal);
		
		parameter.name =REFERENCESPADS_PAR;
		if (ioctl(fd, VL53L0X_IOCTL_PARAMETER, &parameter) < 0) {
			fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER : %s\n", strerror(errno));
			close(fd);
			return -1;
		}
		SpadCount = (uint32_t)parameter.value;
		IsApertureSpads=(uint8_t) parameter.value2;
		fprintf(stderr, "get SpadCount is %d ===\nget IsApertureSpads is %u ===\n", SpadCount,IsApertureSpads);


		//to close
		close(fd);
		return -1;
	}	
	else
	{
		// to init 
		if (ioctl(fd, VL53L0X_IOCTL_INIT , NULL) < 0) {
			fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_INIT : %s\n", strerror(errno));
			close(fd);
			return -1;
		}
	}
	// get data testing
	while (1)
	{
		usleep(30 *1000); /*100ms*/
		// to get all range data
		ioctl(fd, VL53L0X_IOCTL_GETDATAS,&range_datas);
		fprintf(stderr," VL53L0 Range Data:%d, error status:0x%x, signalRate_mcps:%d, Amb Rate_mcps:%d\n",
				range_datas.RangeMilliMeter, range_datas.RangeStatus, range_datas.SignalRateRtnMegaCps, range_datas.AmbientRateRtnMegaCps);
	}
	close(fd);
	return 0;
}


