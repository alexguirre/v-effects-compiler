#include "Effect.h"
#include <d3dcompiler.h>
#include <d3d11.h>
#include <atlbase.h>
#include "EffectInclude.h"
#include "EffectParser.h"

namespace fs = std::filesystem;

CEffect::CEffect(const std::string& source, const fs::path& sourceFilename, const std::vector<fs::path>& includeDirs)
	: mSource(source), mSourceFilename(fs::absolute(sourceFilename)),
	mInclude(std::make_unique<CEffectInclude>(mSourceFilename.parent_path(), includeDirs))
{
	EnsureTechniques();
	EnsureProgramsCode();
}

const CCodeBlob& CEffect::GetProgramCode(const std::string& entrypoint) const
{
	auto e = mProgramsCode.find(entrypoint);
	if (e != mProgramsCode.end())
	{
		return *e->second;
	}
	else
	{
		throw std::invalid_argument("Entrypoint does not exist");
	}
}

std::string CEffect::PreprocessSource() const
{
	CComPtr<ID3DBlob> codeText, errorMsg;
	std::string sourceFileStr = mSourceFilename.string();
	HRESULT r = D3DPreprocess(mSource.c_str(), mSource.size(), sourceFileStr.c_str(), nullptr, mInclude.get(), &codeText, &errorMsg);
	if (SUCCEEDED(r))
	{
		return std::string(reinterpret_cast<const char*>(codeText->GetBufferPointer()), static_cast<size_t>(codeText->GetBufferSize()) - 1); // -1 to exclude null terminator from string length
	}
	else
	{
		throw std::runtime_error(errorMsg ? reinterpret_cast<const char*>(errorMsg->GetBufferPointer()) : "Preprocessor error");
	}
}

void CEffect::EnsureTechniques()
{
	if (!mTechniques.empty())
	{
		return;
	}

	std::string src = PreprocessSource();
	CEffectParser parser(src);
	mTechniques = parser.GetTechniques();

	// TODO: move mSharedVariables and mSamplerState initialization somewhere else
	mSharedVariables = parser.GetSharedVariablesNames();
	mSamplerStates = parser.GetSamplerStates();
}

void CEffect::EnsureProgramsCode()
{
	if (!mProgramsCode.empty())
	{
		return;
	}

	for (int i = 0; i < static_cast<int>(eProgramType::NumberOfTypes); i++)
	{
		eProgramType type = static_cast<eProgramType>(i);

		std::set<std::string> entrypoints;
		GetUsedPrograms(entrypoints, type);

		for (const auto& e : entrypoints)
		{
			mProgramsCode.insert({ e, CompileProgram(e, type) });
		}
	}
}

std::unique_ptr<CCodeBlob> CEffect::CompileProgram(const std::string& entrypoint, eProgramType type) const
{
	// Flags used in the game shaders (except for D3DCOMPILE_NO_PRESHADER, which doesn't seem to be supported in our version of d3dcompile)
	constexpr uint32_t Flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;

	CComPtr<ID3DBlob> code, errorMsg;
	std::string sourceFileStr = mSourceFilename.string();
	HRESULT r = D3DCompile(mSource.c_str(), mSource.size(), sourceFileStr.c_str(), nullptr, mInclude.get(), entrypoint.c_str(), GetTargetForProgram(type), Flags, 0, &code, &errorMsg);
	if (SUCCEEDED(r))
	{
		return std::make_unique<CCodeBlob>(code->GetBufferPointer(), static_cast<uint32_t>(code->GetBufferSize()));
	}
	else
	{
		throw std::runtime_error(errorMsg ? reinterpret_cast<const char*>(errorMsg->GetBufferPointer()) : "Compilation error");
	}
}

const char* CEffect::GetTargetForProgram(eProgramType type)
{
	switch (type)
	{
	case eProgramType::Vertex: return "vs_4_0";
	case eProgramType::Fragment: return "ps_4_0";
	case eProgramType::Compute: return "cs_5_0";
	case eProgramType::Domain: return "ds_5_0";
	case eProgramType::Geometry: return "gs_5_0";
	case eProgramType::Hull: return "hs_5_0";
	}

	throw std::invalid_argument("Invalid program type");
}

const char* CEffect::GetAssignmentTypeForProgram(eProgramType type)
{
	switch (type)
	{
	case eProgramType::Vertex: return "VertexShader";
	case eProgramType::Fragment: return "PixelShader";
	case eProgramType::Compute: return "ComputeShader";
	case eProgramType::Domain: return "DomainShader";
	case eProgramType::Geometry: return "GeometryShader";
	case eProgramType::Hull: return "HullShader";
	}

	throw std::invalid_argument("Invalid program type");
}

void CEffect::GetUsedPrograms(std::set<std::string>& outEntrypoints, eProgramType type) const
{
	outEntrypoints.clear();

	for (auto& t : mTechniques)
	{
		for (auto& p : t.Passes)
		{
			const std::string& shader = p.Shaders[static_cast<int>(type)];
			if (!shader.empty() && shader != CEffect::NullProgramName)
			{
				outEntrypoints.insert(shader);
			}
		}
	}
}

void CEffect::GetPassPrograms(const sTechniquePass& pass, uint8_t outPrograms[static_cast<size_t>(eProgramType::NumberOfTypes)]) const
{
	std::set<std::string> programs;

	for (int i = 0; i < static_cast<int>(eProgramType::NumberOfTypes); i++)
	{
		eProgramType type = static_cast<eProgramType>(i);
		GetUsedPrograms(programs, type);

		const std::string& programName = pass.Shaders[i];

		auto e = programs.find(programName);
		if (e == programs.end())
		{
			outPrograms[i] = 0; // NULL program
		}
		else
		{
			ptrdiff_t index = std::distance(programs.begin(), e) + 1; // +1 because first program is NULL program which is not returned by GetUsedPrograms
			if (index > std::numeric_limits<uint8_t>::max())
			{
				throw std::runtime_error("Program index too big");
			}

			outPrograms[i] = static_cast<uint8_t>(index);
		}
	}
}

CCodeBlob::CCodeBlob(const void* data, uint32_t size)
	: mData(nullptr), mSize(size)
{
	if (data && size > 0)
	{
		mData = std::make_unique<uint8_t[]>(size);
		memcpy_s(mData.get(), mSize, data, mSize);
	}
}

bool sAssignment::IsSamplerStateAssignment(eAssignmentType type)
{
	switch (type)
	{
	case eAssignmentType::AddressU:
	case eAssignmentType::AddressV:
	case eAssignmentType::AddressW:
		return true;

	default:
		return false;
	}
}

sAssignment sAssignment::GetTechniquePassAssignment(const std::string& type, const std::string& value)
{
	sAssignment a = GetAssignment(type, value);
	return !IsSamplerStateAssignment(a.Type) ?
		a :
		throw std::runtime_error("Invalid technique pass assignment type '" + type + "'");
}

sAssignment sAssignment::GetSamplerStateAssignment(const std::string& type, const std::string& value)
{
	sAssignment a = GetAssignment(type, value);
	return IsSamplerStateAssignment(a.Type) ?
		a :
		throw std::runtime_error("Invalid sampler state assignment type '" + type + "'");
}

sAssignment sAssignment::GetAssignment(const std::string& type, const std::string& value)
{
	auto typeEntry = NameToType.find(type);
	if (typeEntry == NameToType.end())
	{
		throw std::runtime_error("Unknown assignment type '" + type + "'");
	}

	eAssignmentType typeId = typeEntry->second;

	auto validAssignmentsEntry = sAssignment::ValidAssignments.find(typeId);
	if (validAssignmentsEntry == sAssignment::ValidAssignments.end())
	{
		throw std::runtime_error("Unknown assignment type id '" + std::to_string(static_cast<uint32_t>(typeId)) + "' ('" + type + "')");
	}

	uint32_t rawValue = 0;
	const sAssignmentValues& validAssignments = validAssignmentsEntry->second;
	if (validAssignments.NamedValues.empty())
	{
		constexpr const char* HexPrefix = "0x";

		// parse a decimal or hexadecimal value
		int base = 10;
		if (value.compare(0, strlen(HexPrefix), HexPrefix) == 0) // is hex
		{
			base = 16;
		}

		rawValue = std::stoul(value, nullptr, base);
	}
	else
	{
		// find the value for the name
		auto valueEntry = validAssignments.NamedValues.find(value);
		if (valueEntry == validAssignments.NamedValues.end())
		{
			throw std::runtime_error("Unknown value '" + value + "' for type '" + type + "'");
		}

		rawValue = valueEntry->second;
	}

	return { typeId, rawValue };
}

const sAssignmentValues sAssignmentValues::Any =
{
	{
		// empty
	}
};

const sAssignmentValues sAssignmentValues::FillMode =
{
	{
		{ "WIREFRAME",	D3D11_FILL_WIREFRAME },
		{ "SOLID",		D3D11_FILL_SOLID },
	}
};

const sAssignmentValues sAssignmentValues::CullMode =
{
	{
		{ "NONE",	D3D11_CULL_NONE },
		{ "FRONT",	D3D11_CULL_FRONT },
		{ "BACK",	D3D11_CULL_BACK },
	}
};

const sAssignmentValues sAssignmentValues::Bool =
{
	{
		{ "FALSE",	false },
		{ "TRUE",	true },
	}
};

const sAssignmentValues sAssignmentValues::DepthWriteMask =
{
	{
		{ "ZERO",	D3D11_DEPTH_WRITE_MASK_ZERO },
		{ "ALL",	D3D11_DEPTH_WRITE_MASK_ALL },
	}
};

const sAssignmentValues sAssignmentValues::ComparisonFunc =
{
	{
		{ "NEVER",			D3D11_COMPARISON_NEVER },
		{ "LESS",			D3D11_COMPARISON_LESS },
		{ "EQUAL",			D3D11_COMPARISON_EQUAL },
		{ "LESS_EQUAL",		D3D11_COMPARISON_LESS_EQUAL },
		{ "GREATER",		D3D11_COMPARISON_GREATER },
		{ "NOT_EQUAL",		D3D11_COMPARISON_NOT_EQUAL },
		{ "GREATER_EQUAL",	D3D11_COMPARISON_GREATER_EQUAL },
		{ "ALWAYS",			D3D11_COMPARISON_ALWAYS },
	}
};

const sAssignmentValues sAssignmentValues::StencilOp =
{
	{
		{ "KEEP",		D3D11_STENCIL_OP_KEEP },
		{ "ZERO",		D3D11_STENCIL_OP_ZERO },
		{ "REPLACE",	D3D11_STENCIL_OP_REPLACE },
		{ "INCR_SAT",	D3D11_STENCIL_OP_INCR_SAT },
		{ "DECR_SAT",	D3D11_STENCIL_OP_DECR_SAT },
		{ "INVERT",		D3D11_STENCIL_OP_INVERT },
		{ "INCR",		D3D11_STENCIL_OP_INCR },
		{ "DECR",		D3D11_STENCIL_OP_DECR },
	}
};

const sAssignmentValues sAssignmentValues::Blend =
{
	{
		{ "ZERO",				D3D11_BLEND_ZERO },
		{ "ONE",				D3D11_BLEND_ONE },
		{ "SRC_COLOR",			D3D11_BLEND_SRC_COLOR },
		{ "INV_SRC_COLOR",		D3D11_BLEND_INV_SRC_COLOR },
		{ "SRC_ALPHA",			D3D11_BLEND_SRC_ALPHA },
		{ "INV_SRC_ALPHA",		D3D11_BLEND_INV_SRC_ALPHA },
		{ "DEST_ALPHA",			D3D11_BLEND_DEST_ALPHA },
		{ "INV_DEST_ALPHA",		D3D11_BLEND_INV_DEST_ALPHA },
		{ "DEST_COLOR",			D3D11_BLEND_DEST_COLOR },
		{ "INV_DEST_COLOR",		D3D11_BLEND_INV_DEST_COLOR },
		{ "SRC_ALPHA_SAT",		D3D11_BLEND_SRC_ALPHA_SAT },
		{ "BLEND_FACTOR",		D3D11_BLEND_BLEND_FACTOR },
		{ "INV_BLEND_FACTOR",	D3D11_BLEND_INV_BLEND_FACTOR },
		{ "SRC1_COLOR",			D3D11_BLEND_SRC1_COLOR },
		{ "INV_SRC1_COLOR",		D3D11_BLEND_INV_SRC1_COLOR },
		{ "SRC1_ALPHA",			D3D11_BLEND_SRC1_ALPHA },
		{ "INV_SRC1_ALPHA",		D3D11_BLEND_INV_SRC1_ALPHA },
	}
};

const sAssignmentValues sAssignmentValues::BlendOp =
{
	{
		{ "ADD",			D3D11_BLEND_OP_ADD },
		{ "SUBTRACT",		D3D11_BLEND_OP_SUBTRACT },
		{ "REV_SUBTRACT",	D3D11_BLEND_OP_REV_SUBTRACT },
		{ "MIN",			D3D11_BLEND_OP_MIN },
		{ "MAX",			D3D11_BLEND_OP_MAX },
	}
};

const sAssignmentValues sAssignmentValues::TextureAddressMode =
{
	{
		{ "WRAP",			D3D11_TEXTURE_ADDRESS_WRAP },
		{ "MIRROR",			D3D11_TEXTURE_ADDRESS_MIRROR },
		{ "CLAMP",			D3D11_TEXTURE_ADDRESS_CLAMP },
		{ "BORDER",			D3D11_TEXTURE_ADDRESS_BORDER },
		{ "MIRROR_ONCE",	D3D11_TEXTURE_ADDRESS_MIRROR_ONCE },
	}
};

const std::unordered_map<eAssignmentType, sAssignmentValues> sAssignment::ValidAssignments =
{
	{ eAssignmentType::FillMode,					sAssignmentValues::FillMode },
	{ eAssignmentType::CullMode,					sAssignmentValues::CullMode },
	{ eAssignmentType::DepthEnable,					sAssignmentValues::Bool },
	{ eAssignmentType::DepthWriteMask,				sAssignmentValues::DepthWriteMask },
	{ eAssignmentType::DepthFunc,					sAssignmentValues::ComparisonFunc },
	{ eAssignmentType::StencilEnable,				sAssignmentValues::Bool },
	{ eAssignmentType::StencilReadMask,				sAssignmentValues::Any },
	{ eAssignmentType::StencilWriteMask,			sAssignmentValues::Any },
	{ eAssignmentType::FrontFaceStencilFail,		sAssignmentValues::StencilOp },
	{ eAssignmentType::FrontFaceStencilDepthFail,	sAssignmentValues::StencilOp },
	{ eAssignmentType::FrontFaceStencilPass,		sAssignmentValues::StencilOp },
	{ eAssignmentType::FrontFaceStencilFunc,		sAssignmentValues::ComparisonFunc },
	{ eAssignmentType::AlphaToCoverageEnable,		sAssignmentValues::Bool },
	{ eAssignmentType::BlendEnable0,				sAssignmentValues::Bool },
	{ eAssignmentType::SrcBlend0,					sAssignmentValues::Blend },
	{ eAssignmentType::DestBlend0,					sAssignmentValues::Blend },
	{ eAssignmentType::BlendOp0,					sAssignmentValues::BlendOp },
	{ eAssignmentType::RenderTargetWriteMask0,		sAssignmentValues::Any },
	{ eAssignmentType::AddressU,					sAssignmentValues::TextureAddressMode },
	{ eAssignmentType::AddressV,					sAssignmentValues::TextureAddressMode },
	{ eAssignmentType::AddressW,					sAssignmentValues::TextureAddressMode },
};

const std::unordered_map<std::string_view, eAssignmentType> sAssignment::NameToType =
{
	{ "FillMode",					eAssignmentType::FillMode },
	{ "CullMode",					eAssignmentType::CullMode },
	{ "DepthEnable",				eAssignmentType::DepthEnable },
	{ "DepthWriteMask",				eAssignmentType::DepthWriteMask },
	{ "DepthFunc",					eAssignmentType::DepthFunc },
	{ "StencilEnable",				eAssignmentType::StencilEnable },
	{ "StencilReadMask",			eAssignmentType::StencilReadMask },
	{ "StencilWriteMask",			eAssignmentType::StencilWriteMask },
	{ "FrontFaceStencilFail",		eAssignmentType::FrontFaceStencilFail },
	{ "FrontFaceStencilDepthFail",	eAssignmentType::FrontFaceStencilDepthFail },
	{ "FrontFaceStencilPass",		eAssignmentType::FrontFaceStencilPass },
	{ "FrontFaceStencilFunc",		eAssignmentType::FrontFaceStencilFunc },
	{ "AlphaToCoverageEnable",		eAssignmentType::AlphaToCoverageEnable },
	{ "BlendEnable0",				eAssignmentType::BlendEnable0 },
	{ "SrcBlend0",					eAssignmentType::SrcBlend0 },
	{ "DestBlend0",					eAssignmentType::DestBlend0 },
	{ "BlendOp0",					eAssignmentType::BlendOp0 },
	{ "RenderTargetWriteMask0",		eAssignmentType::RenderTargetWriteMask0 },
	{ "AddressU",					eAssignmentType::AddressU },
	{ "AddressV",					eAssignmentType::AddressV },
	{ "AddressW",					eAssignmentType::AddressW },
};

const std::unordered_map<eAssignmentType, std::string_view> sAssignment::TypeToName = []()
{
	std::unordered_map<eAssignmentType, std::string_view> tmpMap;
	for (auto& e : sAssignment::NameToType)
	{
		tmpMap.insert({ e.second, e.first });
	}
	return tmpMap;
}();
