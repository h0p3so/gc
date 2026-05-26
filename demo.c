#define GC_IMPLEMENTATION
#define GC_NUM_FLAGS 2
#include "getc.h"

#include <stdio.h>

int main (int argc, char **argv)
{
	struct GCFlag flags[] = {
		{
			.longname = "verbose",
			.shortname = 'v',
			.opts = 0
		},
		{
			.longname = "help",
			.shortname = 'h',
			.opts = 0
		}
	};


	printf("err: %d\n", getc_init(argc, argv, flags).err);
	return 0;
}
