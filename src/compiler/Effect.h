#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <set>
#include <filesystem>
#include <optional>
#include "EffectInclude.h"

struct sTechniquePassAssigment;
struct sTechniquePass;
struct sTechnique;
struct sSamplerState;
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
	std::filesystem::path mSourceFilename;
	std::vector<sTechnique> mTechniques;
	std::vector<std::string> mSharedVariables;
	std::vector<sSamplerState> mSamplerStates;
	std::unordered_map<std::string, std::unique_ptr<CCodeBlob>> mProgramsCode;
	std::unique_ptr<CEffectInclude> mInclude;

public:
	CEffect(const std::string& source, const std::filesystem::path& sourceFilename, const std::vector<std::filesystem::path>& includeDirs);

	void GetUsedPrograms(std::set<std::string>& outEntrypoints, eProgramType type) const;
	const CCodeBlob& GetProgramCode(const std::string& entrypoint) const;
	void GetPassPrograms(const sTechniquePass& pass, uint8_t outPrograms[static_cast<size_t>(eProgramType::NumberOfTypes)]) const;
	std::string PreprocessSource() const;

	inline const std::string& Source() const { return mSource; }
	inline const std::filesystem::path& SourceFilename() const { return mSourceFilename; }
	inline const std::vector<sTechnique>& Techniques() const { return mTechniques; }
	inline const std::vector<std::string>& SharedVariables() const { return mSharedVariables; }
	inline const std::vector<sSamplerState>& SamplerStates() const { return mSamplerStates; }

	static const char* GetTargetForProgram(eProgramType type);
	static const char* GetAssignmentTypeForProgram(eProgramType type);

	static constexpr const char* NullProgramName = "NULL";
private:
	void EnsureTechniques();
	void EnsureProgramsCode();

	std::unique_ptr<CCodeBlob> CompileProgram(const std::string& entryPoint, eProgramType type) const;
};

enum class eAssignmentType : uint32_t
{
	// Rasterizer State
	FillMode = 1,
	CullMode = 6,

	// DepthStencil State
	DepthEnable = 0,
	DepthWriteMask = 2,
	DepthFunc = 7,
	StencilEnable = 11,
	StencilReadMask = 17,
	StencilWriteMask = 18,
	FrontFaceStencilFail = 12,
	FrontFaceStencilDepthFail = 13,
	FrontFaceStencilPass = 14,
	FrontFaceStencilFunc = 15,

	// TODO: BackFace

	// Blend State
	AlphaToCoverageEnable = 32,
	BlendEnable0 = 10,
	SrcBlend0 = 4,
	DestBlend0 = 5,
	BlendOp0 = 23,
	RenderTargetWriteMask0 = 19,

	// TODO: SrcBlendAlpha, DestBlendAlpha, BlendOpAlpha

	
	// Base value for sample state assignments, so they are not 
	// misidentified as technique pass assignments
	SamplerStateOffset = 0x1000,
	
	// Sampler State
	// TODO: missing sampler state assignments
	AddressU = SamplerStateOffset + 0,
	AddressV = SamplerStateOffset + 1,
	AddressW = SamplerStateOffset + 2,
};

struct sAssignmentValues
{
	std::unordered_map<std::string_view, uint32_t> NamedValues;

	static const sAssignmentValues Any, FillMode, CullMode, Bool, DepthWriteMask,
								ComparisonFunc, StencilOp, Blend, BlendOp,
								TextureAddressMode;
};

struct sAssignment
{
	eAssignmentType Type;
	uint32_t Value;

	static const std::unordered_map<eAssignmentType, sAssignmentValues> ValidAssignments;
	static const std::unordered_map<std::string_view, eAssignmentType> NameToType;
	static const std::unordered_map<eAssignmentType, std::string_view> TypeToName;

	static bool IsSamplerStateAssignment(eAssignmentType type);
	static sAssignment GetTechniquePassAssignment(const std::string& type, const std::string& value);
	static sAssignment GetSamplerStateAssignment(const std::string& type, const std::string& value);

private:
	static sAssignment GetAssignment(const std::string& type, const std::string& value);
};

struct sTechniquePass
{
	std::string Shaders[static_cast<size_t>(eProgramType::NumberOfTypes)] =
	{
		CEffect::NullProgramName, CEffect::NullProgramName, CEffect::NullProgramName,
		CEffect::NullProgramName, CEffect::NullProgramName, CEffect::NullProgramName,
	};
	std::vector<sAssignment> Assignments;
};

struct sTechnique
{
	std::string Name;
	std::vector<sTechniquePass> Passes;
};

struct sSamplerState
{
	std::string Name;
	std::vector<sAssignment> Assignments;
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
