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
			.opts = GC_FLAG_NO_ARGUMENT
		},
		{
			.longname = NULL,
			.shortname = 'f',
			.opts = GC_FLAG_MUST_ARGUMENT | GC_ARG_TYPE_TEXT | GC_FLAG_IS_MANDATORY
		}
	};


	printf("err: %d\n", getc_init(argc, argv, flags).err);
	return 0;
}
