#pragma once
#include <d3dcommon.h>
#include <unordered_map>
#include <filesystem>

class CEffectInclude : public ID3DInclude
{
private:
	std::filesystem::path mLocalRootDirectory;
	std::vector<std::vector<char>> mFileBuffers;
	std::unordered_map<uintptr_t, std::filesystem::path> mFileBuffersPaths;

public:
	CEffectInclude(const std::filesystem::path& localRootDirectory);
	CEffectInclude(const CEffectInclude&) = delete;
	CEffectInclude& operator=(const CEffectInclude&) = delete;

	inline const std::filesystem::path& LocalRootDirectory() const { return mLocalRootDirectory; }

	STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override;
	STDMETHOD(Close)(THIS_ LPCVOID pData) override;

public:
	static const std::unordered_map<std::string_view, std::string_view> BuiltinShaders;
};
