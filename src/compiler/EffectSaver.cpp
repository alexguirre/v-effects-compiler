#include "EffectSaver.h"
#include <assert.h>
#include <fstream>

namespace fs = std::filesystem;

static constexpr uint32_t Header = ('r' << 0) | ('g' << 8) | ('x' << 16) | ('e' << 24);

CEffectSaver::CEffectSaver(const CEffect& effect)
	: mEffect(effect)
{
}

void CEffectSaver::SaveTo(const fs::path& filePath) const
{
	if (!filePath.has_filename())
	{
		throw std::invalid_argument("Path '" + filePath.string() + "' is not a valid file path");
	}

	fs::path fullPath = fs::absolute(filePath);
	if (!fs::exists(fullPath.parent_path()))
	{
		throw std::invalid_argument("Parent path '" + fullPath.parent_path().string() + "' does not exist");
	}

	std::ofstream f(fullPath, std::ios_base::out | std::ios_base::binary);

	/* Header */
	WriteUInt32(f, Header);
	WriteUInt32(f, 0xDEADBEEF); // TODO: vertex type

	/* Annotations */
	WriteUInt8(f, 0); // no annotations support for now

	// TODO: finish CEffectSaver::SaveTo
}

void CEffectSaver::WriteLengthPrefixedString(std::ostream& o, const std::string& str) const
{
	size_t length = str.size() + 1; // + null terminator
	if (length > 255)
	{
		throw std::invalid_argument("String too long");
	}

	WriteUInt8(o, static_cast<uint8_t>(length));
	o.write(str.c_str(), str.size());
	WriteUInt8(o, 0); // null terminator
}

void CEffectSaver::WriteUInt32(std::ostream& o, uint32_t v) const
{
	o.write(reinterpret_cast<char*>(&v), sizeof(uint32_t));
}

void CEffectSaver::WriteUInt16(std::ostream& o, uint16_t v) const
{
	o.write(reinterpret_cast<char*>(&v), sizeof(uint16_t));
}

void CEffectSaver::WriteUInt8(std::ostream& o, uint8_t v) const
{
	o.write(reinterpret_cast<char*>(&v), sizeof(uint8_t));
}
