#include "libbb.h"
#include "match_class.h"

int match_class(const char *name)
{
	return index_in_strings(CHAR_CLASSES, name);
}
