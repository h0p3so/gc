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
			.opts = GC_OPTS_FLAG_NON_ARG
		},
		{
			.longname = "file",
			.shortname = 'f',
			.opts = GC_OPTS_FLAG_REQ_ARG | GC_OPTS_FLAG_ARG_TYPE_TEXT
		}
	};


	printf("err: %d\n", getc_init(argc, argv, flags, "testing").err);
	return 0;
}
