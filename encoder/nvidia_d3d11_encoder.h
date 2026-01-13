#pragma once

#include "video_encoder.h"

namespace xop {

class NvidiaD3D11Encoder : public VideoEncoder
{
public:
	NvidiaD3D11Encoder();
	virtual ~NvidiaD3D11Encoder();

	static bool IsSupported();

	virtual bool Init() override;
	virtual void Destroy()override;

	virtual int  Encode(std::vector<uint8_t> in_image, std::vector<uint8_t>& out_frame);

	//virtual int  Encode(HANDLE shared_handle, std::vector<uint8_t>& out_frame);

private:
	bool UpdateOption();
	void UpdateEvent();
	bool InitD3D11();
	void ClearD3D11();

	int gpu_index_     = 0;
	int width_         = 1920;
	int height_        = 1080;
	int bitrate_kbps_  = 8000;
	int frame_rate_    = 30;
	int gop_           = 300;
	int dxgi_format_   = 87;
	int codec_         = 1;
	
};

}
