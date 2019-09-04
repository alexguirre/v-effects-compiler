#include "EffectInclude.h"
#include <stdexcept>
#include <fstream>

namespace fs = std::filesystem;

CEffectInclude::CEffectInclude(const fs::path& localRootDirectory)
	: mLocalRootDirectory(fs::absolute(localRootDirectory))
{
	if (!fs::is_directory(mLocalRootDirectory))
	{
		throw std::invalid_argument("Local root directory path '" + mLocalRootDirectory.string() + "' is not a directory");
	}
}

HRESULT CEffectInclude::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
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
		const fs::path& rootDir = pParentData ? 
			mFileBuffers.at(reinterpret_cast<uintptr_t>(pParentData)).Path.parent_path() :
			mLocalRootDirectory;

		fs::path filePath = fs::canonical(rootDir / pFileName);
		if (fs::is_regular_file(filePath))
		{
			// open file
			std::ifstream file(filePath, std::ios::binary | std::ios::in | std::ios::ate); // open at the end to get the size with tellg()
			std::streamsize fileSize = file.tellg();
			file.seekg(0, std::ios::beg);

			// read file into buffer
			sFileBuffer f;
			f.Buffer.resize(fileSize);
			file.read(f.Buffer.data(), fileSize);
			
			// save the path for this file in case it includes more files relative to it
			f.Path = filePath;

			*ppData = f.Buffer.data();
			*pBytes = static_cast<UINT>(f.Buffer.size());

			const uintptr_t key = reinterpret_cast<uintptr_t>(f.Buffer.data());
			mFileBuffers.try_emplace(key, std::move(f));

			return S_OK;
		}

		break;
	}
	}

	*ppData = nullptr;
	*pBytes = 0;
	return E_FAIL;
}

HRESULT CEffectInclude::Close(LPCVOID pData)
{
	// search for the buffer with the same data pointer and delete it
	auto toDelete = mFileBuffers.find(reinterpret_cast<uintptr_t>(pData));
	if (toDelete != mFileBuffers.cend())
	{
		mFileBuffers.erase(toDelete);
	}

	return S_OK;
}

// TODO: read builtin shaders from external files instead of embedding them
const std::unordered_map<std::string_view, std::string_view> CEffectInclude::BuiltinShaders =
{
	{ 
		"global_buffers.fxh", 
#include "global_buffers.fxh"
	}
};
