#pragma once
#include <d3dcommon.h>
#include <unordered_map>
#include <filesystem>

class CEffectInclude : public ID3DInclude
{
private:
	struct sFileBuffer
	{
		std::vector<char> Buffer;
		std::filesystem::path Path;

		sFileBuffer() = default;
		sFileBuffer(sFileBuffer&&) = default;
		sFileBuffer& operator=(sFileBuffer&&) = default;
		sFileBuffer(const sFileBuffer&) = delete;
		sFileBuffer& operator=(const sFileBuffer&) = delete;
		
	};

	std::filesystem::path mLocalRootDirectory;
	std::unordered_map<uintptr_t, sFileBuffer> mFileBuffers;

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
