/*
 * MCP9808 Digital Temperature Sensor
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <errno.h>

#include "mcp9808.h"

/*
 * public functions
 */

int
mcp9808_config(int fd, uint8_t slave_addr)
{
	if (ioctl(fd, I2C_SLAVE, slave_addr) == -1) {
		fprintf(stderr,
			"ioctl(I2C_SLAVE, 0x%02X): %s\n",
			slave_addr, strerror(errno));
		return -1;
	}
	return 0;
}
