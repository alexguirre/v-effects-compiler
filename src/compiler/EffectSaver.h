#pragma once
#include <filesystem>
#include <ostream>

class CEffect;
enum class eProgramType;

class CEffectSaver
{
private:
	const CEffect& mEffect;

public:
	CEffectSaver(const CEffect& effect);

	void SaveTo(const std::filesystem::path& filePath) const;

private:
	void WriteHeader(std::ostream& o) const;
	void WriteAnnotations(std::ostream& o) const;
	void WritePrograms(std::ostream& o, eProgramType type) const;
	void WriteBuffers(std::ostream& o, bool globals) const;

	void WriteNullProgram(std::ostream& o) const;

	void WriteLengthPrefixedString(std::ostream& o, const std::string& str) const;
	void WriteUInt32(std::ostream& o, uint32_t v) const;
	void WriteUInt16(std::ostream& o, uint16_t v) const;
	void WriteUInt8(std::ostream& o, uint8_t v) const;
};
