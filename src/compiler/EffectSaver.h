#pragma once
#include <filesystem>
#include <ostream>

class CEffect;

class CEffectSaver
{
private:
	const CEffect& mEffect;

public:
	CEffectSaver(const CEffect& effect);

	void SaveTo(const std::filesystem::path& filePath) const;

private:
	void WriteLengthPrefixedString(std::ostream& o, const std::string& str) const;
	void WriteUInt32(std::ostream& o, uint32_t v) const;
	void WriteUInt16(std::ostream& o, uint16_t v) const;
	void WriteUInt8(std::ostream& o, uint8_t v) const;
};

