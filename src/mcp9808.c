/*
 * MCP9808 Digital Temperature Sensor
 */
/*

bang: DIY thermostat, designed to run on a Raspberry Pi, or any Linux
      system with GPIO and an I2C bus.
Copyright (C) 2019 Todd Allen

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of  MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.

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
mcp9808_read_temp(int fd, uint16_t *raw, double *temp)
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
		uint16_t ut;
		int st;

		/*
		 * 13-bit signed, in 16ths of degree C (big-endian)
		 * first 3 bits are flags, ignored
		 */
		ut = ((rbuf[0] & 0x1F) << 8) | rbuf[1];
		if (rbuf[0] & 0x10)
			st = (int)ut - (1U << 13); /* negative */
		else
			st = ut;                   /* positive */

		*temp = (double)st / 16.0;
	}

	return 0;
}
