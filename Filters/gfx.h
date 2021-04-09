#pragma once
#include <wtypes.h>
#include <cinttypes>

class Gfx {
private:
	BYTE* buffer;
	LONG width;
	LONG height;

public:
	Gfx(BYTE* buffer, LONG width, LONG height) :
		buffer(buffer), width(width), height(height){}

	void putPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
	void putText(int x, int y, const char* text, uint8_t r, uint8_t g, uint8_t b);
	void putRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
};