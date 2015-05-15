#ifndef _SH_H
#define _SH_H

#define __PROGNAME		"sh"
#define __PROGVER		"1.0"


#define ERR_NONE		0
#define ERR_USAGE		1
#define ERR_HELP		2
#define ERR_CMDARG		3
#define ERR_ARG			4
#define ERR_KBD			5

static char* __errors[] = {
	"",
	"usage: " __PROGNAME " [options]",
	"help: " __PROGNAME " blabla...",
	"-c requieres an argument",
	"Invalid arguments",
	"No input device found",
};


#define _EC(x)	extern x (char**)

_EC(cmd_cd);
_EC(cmd_exit);

#endif

