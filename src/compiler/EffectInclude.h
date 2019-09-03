#pragma once
#include <d3dcommon.h>
#include <unordered_map>
#include <memory>

class CEffectInclude : public ID3DInclude
{
public:
	CEffectInclude(const CEffectInclude&) = delete;
	CEffectInclude& operator=(const CEffectInclude&) = delete;

	STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override;
	STDMETHOD(Close)(THIS_ LPCVOID pData) override;

private:
	CEffectInclude() {}

public:
	static std::shared_ptr<CEffectInclude> Instance();

	static const std::unordered_map<std::string_view, std::string_view> BuiltinShaders;
};

