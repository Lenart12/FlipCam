#include "config.h"
#include <vector>

const std::vector<std::wstring> explode(const std::wstring& s, const wchar_t& c) {
	std::wstring buff = L"";
	std::vector<std::wstring> v;

	for (auto n : s){
		if (n != c)
			buff += n;
		else if (n == c && buff != L"") { 
			v.push_back(buff);
			buff = L"";
		}
	}
	if (buff != L"") v.push_back(buff);

	return v;
}

FlipCamConfig::FlipCamConfig(){}

FlipCamConfig::FlipCamConfig(const std::wstring& cfg)
{
	auto tokens = explode(cfg, ',');
	for (auto& token : tokens) {
		auto kvp = explode(token, ':');
		if (kvp.size() == 2) {
			// res:WxH
			if (kvp[0] == L"res") {
				auto res = explode(kvp[1], 'x');
				if (res.size() == 2) {
					this->width  = std::stoi(res[0]) / 4;
					this->height = std::stoi(res[1]) / 4;
				}
				else if (res.size() == 3) {
					this->width = std::stoi(res[0]) / 4;
					this->height = std::stoi(res[1]) / 4;
					this->timePerFrame = 10000000 / std::stoi(res[2]);
				}
			} 
		} else if (kvp.size() == 1) {
			if (kvp[0] == L"vflip") {
				this->vFlip = true;
			}
			else if (kvp[0] == L"hflip") {
				this->hFlip = true;
			}
			else if (kvp[0] == L"debug") {
				this->debug = true;
			}
			else if (kvp[0] == L"dvd") {
				this->dvd = true;
			}
		}
	}
}
