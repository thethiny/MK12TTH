#include "prettyprint.h"

void printfColor(const char* color, const char* const Format, ...)
{
	SetColor(color);

	va_list args;
	va_start(args, Format);
	vfprintf(stdout, Format, args);
	va_end(args);

	ResetColors();
}

void SetColor(const char* color)
{
	printf(color);
}