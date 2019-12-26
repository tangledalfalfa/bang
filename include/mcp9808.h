/*
 * Header file for MCP9808 Digital Temperature Sensor
 */

#ifndef MCP9808_H_
#define MCP9808_H_

#include <stdint.h>

/*
 * public function prototypes
 */

int
mcp9808_config(int fd, uint8_t slave_addr);

#endif
