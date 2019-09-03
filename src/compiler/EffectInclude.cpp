#include "EffectInclude.h"
#include <stdexcept>

#include <iostream>

HRESULT CEffectInclude::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID /* pParentData */, LPCVOID* ppData, UINT* pBytes)
{
	switch (IncludeType)
	{
	case D3D_INCLUDE_SYSTEM:
	{
		auto e = BuiltinShaders.find(pFileName);
		if (e != BuiltinShaders.end())
		{
			*ppData = e->second.data();
			*pBytes = static_cast<UINT>(e->second.length());
			return S_OK;
		}

		break;
	}

	case D3D_INCLUDE_LOCAL:
	{
		// TODO: local includes are not supported for now

		break;
	}
	}

	*ppData = nullptr;
	*pBytes = 0;
	return E_FAIL;
}

HRESULT CEffectInclude::Close(LPCVOID /* pData */)
{
	// nothing to do
	return S_OK;
}

std::shared_ptr<CEffectInclude> CEffectInclude::Instance()
{
	static std::shared_ptr<CEffectInclude> instance{ new CEffectInclude };
	return instance;
}

// TODO: read builtin shaders from external files instead of embedding them
const std::unordered_map<std::string_view, std::string_view> CEffectInclude::BuiltinShaders =
{
	{ 
		"global_buffers.fxh", 
#include "global_buffers.fxh"
	}
};
