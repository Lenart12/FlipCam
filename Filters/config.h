#pragma once
#include <string>
#include <streams.h>

class FlipCamConfig {
public:
	int width  = 320; // actual width  = 320 * 4 = 1280
	int height = 180; // actual height = 180 * 4 = 720
	REFERENCE_TIME timePerFrame = 416666; //       24 FPS

	bool vFlip = false;
	bool hFlip = false;

	bool debug = false;

	FlipCamConfig();
	FlipCamConfig(const std::wstring& cfg);
};