/*
 * MCP9808 Digital Temperature Sensor
 */

#include "config.h"

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <errno.h>

#include "mcp9808.h"
#include "util.h"

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

int
mcp9808_read_temp(int fd, uint16_t *raw, float *temp)
{
	static const uint8_t addr_t_amb = 0x05;
	uint8_t rbuf[2];

	/* write command */
        if (writen(fd, &addr_t_amb, sizeof addr_t_amb) == -1)
		return -1;

	/* read result */
	if (readn(fd, rbuf, sizeof rbuf) == -1)
		return -1;

	if (raw != NULL)
		*raw = (rbuf[0] << 8) | rbuf[1];

	if (temp != NULL) {
		rbuf[0] &= 0x1F; /* clear flags */

		if (rbuf[0] & 0x10) { /* sign bit */
			rbuf[0] &= 0x0F;
			*temp = 256.0f - (rbuf[0] * 16.0f + rbuf[1] / 16.0f);
		} else {
			*temp = rbuf[0] * 16.0f + rbuf[1] / 16.0f;
		}
	}

	return 0;
}
