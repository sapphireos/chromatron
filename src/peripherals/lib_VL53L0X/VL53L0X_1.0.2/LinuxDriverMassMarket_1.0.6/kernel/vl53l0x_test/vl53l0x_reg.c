

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

/*
 *  IOCTL register data structs
 */
struct stmvl53l0x_register {
	uint32_t is_read; //1: read 0: write
	uint32_t reg_index;
	uint32_t reg_bytes;
	uint32_t reg_data;
	int32_t status;
};
//******************************** IOCTL definitions
#define VL53L0X_IOCTL_REGISTER		_IOWR('p', 0x0c, struct stmvl53l0x_register)

static void help(void)
{
	fprintf(stderr,
		"Usage: vl53l0x_reg REG_ADDR REG_BYTES [REG_DATA]\n"
		" REG_ADDR is VL6180's register address using 0xxx format: \n"
		" REG_BYTES is the register bytes(1, 2 or 4) \n"
		" REG_DATA is optional for writting data to requested REG_ADDR\n"
		);
	exit(1);
}

int main(int argc, char *argv[])
{
	int fd;
	struct stmvl53l0x_register reg;
	char *end;

	if (argc < 3) {
		help();
		exit(1);
	}

	fd = open("/dev/stmvl53l0x_ranging",O_RDWR );
	if (fd <= 0) {
		fprintf(stderr,"Error open stmvl53l0x_ranging device: %s\n", strerror(errno));
		return -1;
	}

	reg.reg_index = strtoul(argv[1], &end, 16);
	if (*end) {
		help();
		exit(1);
	}
 	reg.reg_bytes = strtoul(argv[2],&end,10);
	if (*end) {
	   	help();
		exit(1);
	}
	if (argc == 3) {
		reg.is_read = 1;
		fprintf(stderr, "To read VL53L0 register index:0x%x, bytes:%d\n",
						reg.reg_index, reg.reg_bytes);

	} else {
		reg.is_read = 0;
		reg.reg_data = strtoul(argv[3],&end,16);
		if (*end) {
	   		help();
			exit(1);
		}
		fprintf(stderr, "To write VL53L0 register index:0x%x, bytes:%d as value:0x%x\n",
						reg.reg_index, reg.reg_bytes, reg.reg_data);				
	}
	// to access requested register
	ioctl(fd, VL53L0X_IOCTL_REGISTER,&reg);
	fprintf(stderr," VL53L0 register data:0x%x, error status:%d\n",
				reg.reg_data, reg.status);
	
	//close(fd);
	return 0;
}


