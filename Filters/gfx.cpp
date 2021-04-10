#include "gfx.h"
#include "font.h"

void Gfx::putPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	if (x < 0 || x >= width || y < 0 || y >= height)
		return;
	int bufferIndex = (((height - 1) - y) * width + x) * 3;
	buffer[bufferIndex + 2] = r;
	buffer[bufferIndex + 1] = g;
	buffer[bufferIndex + 0] = b;
}

void Gfx::putText(int x0, int y0, const char* text, uint8_t r, uint8_t g, uint8_t b, bool background)
{
	uint8_t font_w = FontUbuntu[0];
	uint8_t font_h = FontUbuntu[1];
	uint8_t char_start = FontUbuntu[2];
	uint8_t char_length = FontUbuntu[3];
	int char_size = font_h * font_w / 8;
	const uint8_t line_spacing = 5;

	int runningX = x0;
	int runningY = y0;

	for (char const *c = text; *c != '\0'; c++) {
		if (*c == '\n') {
			runningX = x0;
			runningY += font_h + line_spacing;
		}
		else if (*c - char_start <= char_length) {
			uint8_t const* glyph = (uint8_t*)FontUbuntu + 4 + (*c - char_start) * char_size;
			
			for (uint8_t y = 0; y < font_h; y++) {
				for (uint8_t x = 0; x < font_w; x++) {
					int bitI = 7 - (x % 8);
					int byteI = (font_w / 8) * y + x / 8;
					if (*(glyph + byteI) & (1 << bitI)) {
						putPixel(runningX + x, runningY + y, r, g, b);
					}
					else if (background) {
						putPixel(runningX + x, runningY + y, 255-r, 255-g, 255-b);
					}
				}
			}
			runningX += font_w;
		}
	}
}

void Gfx::putRect(int x0, int y0, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
	for (int y = y0; y < y0 + h; y++) {
		for (int x = x0; x < x0 + w; x++) {
			putPixel(x, y, r, g, b);
		}
	}
}

void Gfx::ingest(BYTE* pData, LONG oWidth, LONG oHeight, bool vFlip, bool hFlip) {
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			//long bufferFrom = (((oHeight - 1) - (y/height)*oHeight) * oWidth + (x/width)*oWidth) * 3;
			long bufferFrom = 0;
			long bufferTo = (((height - 1) - y) * width + x) * 3;

			buffer[bufferTo + 2] = pData[bufferFrom + 2];
			buffer[bufferTo + 1] = pData[bufferFrom + 1];
			buffer[bufferTo + 0] = pData[bufferFrom + 0];
		}
	}

}

void Gfx::fillScren(uint8_t r, uint8_t g, uint8_t b)
{
	size_t buffSize = (size_t)width * 3 * height;
	for (size_t i = 0; i < buffSize; i += 3) {
		buffer[i + 2] = r;
		buffer[i + 1] = g;
		buffer[i]     = b;
	}
}
