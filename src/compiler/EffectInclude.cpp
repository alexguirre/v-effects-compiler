#include "EffectInclude.h"
#include <stdexcept>
#include <fstream>

namespace fs = std::filesystem;

CEffectInclude::CEffectInclude(const fs::path& localRootDirectory, const std::vector<fs::path>& includeDirs)
	: mLocalRootDirectory(fs::absolute(localRootDirectory))
{
	if (!fs::is_directory(mLocalRootDirectory))
	{
		throw std::invalid_argument("Local root directory path '" + mLocalRootDirectory.string() + "' is not a directory");
	}

	std::transform(
		includeDirs.begin(), includeDirs.end(),
		std::back_inserter(mIncludeDirectories),
		[](auto& p) { return fs::absolute(p); }
	);
}

HRESULT CEffectInclude::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
{
	fs::path filePath;
	bool foundFile = false;

	switch (IncludeType)
	{
	case D3D_INCLUDE_SYSTEM:
	{
		// search for the file in the include directories
		for (const auto& includeDir : mIncludeDirectories)
		{
			filePath = fs::weakly_canonical(includeDir / pFileName);
			if (fs::is_regular_file(filePath))
			{
				foundFile = true;
				break;
			}
		}

		break;
	}

	case D3D_INCLUDE_LOCAL:
	{
		// get the file relative to the parent file
		const fs::path& rootDir = pParentData ? 
			mFileBuffers.at(reinterpret_cast<uintptr_t>(pParentData)).Path.parent_path() :
			mLocalRootDirectory;

		filePath = fs::weakly_canonical(rootDir / pFileName);
		foundFile = true;

		break;
	}
	}

	if (foundFile && fs::is_regular_file(filePath))
	{
		const sFileBuffer& f = OpenFile(filePath);

		*ppData = f.Buffer.data();
		*pBytes = static_cast<UINT>(f.Buffer.size());

		return S_OK;
	}

	*ppData = nullptr;
	*pBytes = 0;
	return E_FAIL;
}

HRESULT CEffectInclude::Close(LPCVOID pData)
{
	return CloseFile(reinterpret_cast<uintptr_t>(pData)) ? S_OK : E_FAIL;
}
const CEffectInclude::sFileBuffer& CEffectInclude::OpenFile(const std::filesystem::path& filePath)
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

	const uintptr_t key = reinterpret_cast<uintptr_t>(f.Buffer.data());
	auto at = mFileBuffers.try_emplace(key, std::move(f)).first;
	return at->second;
}

bool CEffectInclude::CloseFile(uintptr_t key)
{
	// search for the buffer with the same data pointer and delete it
	auto toDelete = mFileBuffers.find(key);
	if (toDelete != mFileBuffers.cend())
	{
		mFileBuffers.erase(toDelete);
		return true;
	}
	else
	{
		return false;
	}
}
