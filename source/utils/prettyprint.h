#pragma once
#include <cstdarg>
#include <string>

void printfColor(const char* color, const char* const Format, ...);
void SetColor(const char* color);

enum ConsoleColors
{
	BLACK = 0,
	BLUE,
	GREEN,
	AQUA,
	RED,
	PURPLE,
	YELLOW,
	WHITE,
	GRAY,
	LIGHTBLUE,
	LIGHTGREEN,
	LIGHTAQUA,
	LIGHTRED,
	LIGHTPURPLE,
	LIGHTYELLOW,
	BRIGHTWHITE,

	GREY = 8, // Synonym
};

#ifndef COLORPRINT
#define COLORPRINT
#define printfRed(Format, ...) printfColor("\x1b[31m", Format, ## __VA_ARGS__)
#define printfGreen(Format, ...) printfColor("\x1b[32m", Format, ## __VA_ARGS__)
#define printfYellow(Format, ...) printfColor("\x1b[33m", Format, ## __VA_ARGS__)
#define printfBlue(Format, ...) printfColor("\x1b[34m", Format, ## __VA_ARGS__)
#define printfCyan(Format, ...) printfColor("\x1b[36m", Format, ## __VA_ARGS__)
#define printfError(Format, ...) printfColor("\x1b[41m", Format, ## __VA_ARGS__)
#define printfSuccess(Format, ...) printfColor("\x1b[42m\x1b[30m", Format, ## __VA_ARGS__)
#define printfWarning(Format, ...) printfColor("\x1b[43m\x1b[30m", Format, ## __VA_ARGS__)
#define printfInfo(Format, ...) printfColor("\x1b[46m\x1b[30m", Format, ## __VA_ARGS__)

#define SetColorRed() SetColor("\x1b[31m")
#define SetColorGreen() SetColor("\x1b[32m")
#define SetColorYellow() SetColor("\x1b[33m")
#define SetColorBlue() SetColor("\x1b[34m")
#define SetColorCyan() SetColor("\x1b[36m")
#define SetColorError() SetColor("\x1b[41m")
#define SetColorSuccess() SetColor("\x1b[42m\x1b[30m")
#define SetColorWarning() SetColor("\x1b[43m\x1b[30m")
#define SetColorInfo() SetColor("\x1b[46m\x1b[30m")
#define ResetColors() SetColor("\033[0m")
#endif