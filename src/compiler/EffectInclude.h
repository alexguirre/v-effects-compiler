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
	std::vector<std::filesystem::path> mIncludeDirectories;
	std::unordered_map<uintptr_t, sFileBuffer> mFileBuffers;

public:
	CEffectInclude(const std::filesystem::path& localRootDirectory, const std::vector<std::filesystem::path>& includeDirs);
	CEffectInclude(const CEffectInclude&) = delete;
	CEffectInclude& operator=(const CEffectInclude&) = delete;

	inline const std::filesystem::path& LocalRootDirectory() const { return mLocalRootDirectory; }
	inline const std::vector<std::filesystem::path>& IncludeDirectories() const { return mIncludeDirectories; }

	STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override;
	STDMETHOD(Close)(THIS_ LPCVOID pData) override;

private:
	const sFileBuffer& OpenFile(const std::filesystem::path& filePath);
	bool CloseFile(uintptr_t key);
};
