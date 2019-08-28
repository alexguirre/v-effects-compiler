#pragma once
#include <string>
#include <vector>
#include <memory>

struct sTechniquePassAssigment;
struct sTechniquePass;
struct sTechnique;
class CCodeBlob;

enum class eProgramType
{
	Vertex,
	Fragment,
	Compute,
	Domain,
	Geometry,
	Hull,
};

class CEffect
{
private:
	std::string mSource;
	std::vector<sTechnique> mTechniques;

public:
	CEffect(const std::string& source);

	std::unique_ptr<CCodeBlob> CompileProgram(const std::string& entryPoint, eProgramType type) const;

	inline const std::string& Source() const { return mSource; }
	inline const std::vector<sTechnique>& Techniques() const { return mTechniques; }

private:
	void EnsureTechniques();
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

	inline uint32_t Size() const { return mSize; }
};
