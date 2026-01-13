#include "nvidia_d3d11_encoder.h"

namespace xop
{

class NvidiaRAII
{
public:
	NvidiaRAII() {}

	~NvidiaRAII() {	}

private:
};

NvidiaD3D11Encoder::NvidiaD3D11Encoder()
{

}

NvidiaD3D11Encoder::~NvidiaD3D11Encoder()
{
	Destroy();
}

bool NvidiaD3D11Encoder::IsSupported()
{
	return false;
}

bool NvidiaD3D11Encoder::Init()
{
	return true;
}

void NvidiaD3D11Encoder::Destroy()
{
	ClearD3D11();
}

int NvidiaD3D11Encoder::Encode(std::vector<uint8_t> in_image, std::vector<uint8_t>& out_frame)
{
	return 0;
}


bool NvidiaD3D11Encoder::InitD3D11()
{
	ClearD3D11();
	
	return true;
}

void NvidiaD3D11Encoder::ClearD3D11()
{

}

bool NvidiaD3D11Encoder::UpdateOption()
{
	return true;
}

void NvidiaD3D11Encoder::UpdateEvent()
{

}

}
