#include <stdio.h>

#include "cmds.h"

int cmd_version(int argc, char *argv[])
{
	printf("bcachefs tool version %s\n", VERSION_STRING);
	return 0;
}
