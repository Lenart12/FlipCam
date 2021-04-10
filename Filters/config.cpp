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

IBaseFilter* getDeviceByFriendlyName(const std::wstring& name) {
	// Create the System Device Enumerator.
	IBaseFilter* pSourceF = NULL;

	ICreateDevEnum* pDevEnum;
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
		CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
	if (FAILED(hr)) goto done;

	IEnumMoniker* pEnum = NULL;
	hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
	pDevEnum->Release();
	if (FAILED(hr)) goto done;

	IMoniker* pMoniker = NULL;

	while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {
		IPropertyBag* pPropBag;
		HRESULT hr = pMoniker->BindToStorage(0, NULL, IID_PPV_ARGS(&pPropBag));
		if (FAILED(hr))
		{
			pMoniker->Release();
			continue;
		}

		VARIANT var;
		VariantInit(&var);

		// Get description or friendly name.
		hr = pPropBag->Read(L"FriendlyName", &var, 0);
		if (FAILED(hr))	{
			hr = pPropBag->Read(L"Description", &var, 0);
		}
		bool toBreak = false;
		if (SUCCEEDED(hr))
		{
			if (lstrcmpW(name.c_str(), var.bstrVal) == 0) {
				hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pSourceF);
				if (SUCCEEDED(hr)) {
					toBreak = true;
				}
			}
		}
		VariantClear(&var);
		pPropBag->Release();
		pMoniker->Release();
		if (toBreak) break;
	}

	pEnum->Release();
done:
	return pSourceF;
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
			// device:friendly name
			else if (kvp[0] == L"device") {
				this->webcamSource = getDeviceByFriendlyName(kvp[1]);
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
