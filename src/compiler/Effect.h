#pragma once
#include <string>
#include <vector>

struct sTechniquePassAssigment;
struct sTechniquePass;
struct sTechnique;

class CEffect
{
private:
	std::string mSource;
	std::vector<sTechnique> mTechniques;

public:
	CEffect(const std::string& source);

	void EnsureTechniques();

	inline const std::string& Source() const { return mSource; }
	inline const std::vector<sTechnique>& Techniques() const { return mTechniques; }
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
