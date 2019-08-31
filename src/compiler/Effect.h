#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <set>

struct sTechniquePassAssigment;
struct sTechniquePass;
struct sTechnique;
class CCodeBlob;

enum class eProgramType
{
	// Keep these ordered, programs in FXC have this order

	Vertex = 0,
	Fragment,
	Compute,
	Domain,
	Geometry,
	Hull,

	NumberOfTypes,
};

class CEffect
{
private:
	std::string mSource;
	std::vector<sTechnique> mTechniques;
	std::unordered_map<std::string, std::unique_ptr<CCodeBlob>> mProgramsCode;

public:
	CEffect(const std::string& source);

	void GetUsedPrograms(std::set<std::string>& outEntrypoints, eProgramType type) const;
	const CCodeBlob& GetProgramCode(const std::string& entrypoint) const;
	void GetPassPrograms(const sTechniquePass& pass, uint8_t outPrograms[static_cast<size_t>(eProgramType::NumberOfTypes)]) const;
	std::string PreprocessSource() const;

	inline const std::string& Source() const { return mSource; }
	inline const std::vector<sTechnique>& Techniques() const { return mTechniques; }

private:
	void EnsureTechniques();
	void EnsureProgramsCode();

	std::unique_ptr<CCodeBlob> CompileProgram(const std::string& entryPoint, eProgramType type) const;
};

struct sAssigment
{
	std::string Type;
	std::string Value;
};

struct sTechniquePass
{
	std::vector<sAssigment> Assigments;
};

struct sTechnique
{
	std::string Name;
	std::vector<sTechniquePass> Passes;
};

class CCodeBlob
{
private:
	std::unique_ptr<uint8_t[]> mData;
	uint32_t mSize;

public:
	CCodeBlob(const void* data, uint32_t size);

	inline const uint8_t* Data() const { return mData.get(); }
	inline uint32_t Size() const { return mSize; }
};
