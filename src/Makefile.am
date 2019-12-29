bin_PROGRAMS = bang
bang_SOURCES = bang.c mcp9808.c util.c thermostat.c schedule.c cfgfile.c
AM_CFLAGS = -I$(top_srcdir)/include
AM_CFLAGS += -g -std=c99 -pedantic -Wall -Wextra -Wmissing-prototypes
bang_LDADD = -lgpiod -lconfig
# AM_LDFLAGS
#LDADD = lgpiod
