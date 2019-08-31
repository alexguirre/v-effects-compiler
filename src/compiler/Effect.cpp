#include "Effect.h"
#include <tao/pegtl.hpp>
#include <tao/pegtl/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>
#include <d3dcompiler.h>
#include <atlbase.h>

namespace pegtl = tao::TAO_PEGTL_NAMESPACE;

// Contains the PEGTL grammar for parsing effect techniques
namespace technique
{
	using namespace pegtl;

	struct str_technique : TAO_PEGTL_STRING("technique") {};
	struct str_pass : TAO_PEGTL_STRING("pass") {};

	struct comment : seq<two<'/'>, until<eolf>> {};
	struct sp : sor<space, comment> {};
	struct sp_s : star<sp> {};
	struct sp_p : plus<sp> {};

	struct integer_dec : plus<digit> {};
	struct integer_hex : seq<one<'0'>, one<'x'>, plus<xdigit>> {};
	struct integer : sor<integer_hex, integer_dec> {};

	template<typename Rule>
	struct braces : if_must<one<'{'>, sp_s, Rule, sp_s, one<'}'>> {};

	struct pass_assignment_name : identifier {};
	struct pass_assignment_value_string : identifier {};
	struct pass_assignment_value_integer : integer {};
	struct pass_assignment_value : sor<pass_assignment_value_integer, pass_assignment_value_string> {};
	struct pass_assignment : seq<pass_assignment_name, sp_s, one<'='>, sp_s, pass_assignment_value, sp_s, one<';'>, sp_s> {};
	struct pass : seq<str_pass, sp_s, braces<star<pass_assignment>>, sp_s> {};

	struct technique_name : identifier {};
	struct technique : seq<str_technique, sp_p, technique_name, sp_s, braces<star<pass>>, sp_s> {};

	struct grammar : star<until<technique>> {};

	struct state
	{
		int Depth = 0;
		sTechnique CurrentTechnique;
		sTechniquePass CurrentPass;
		sAssigment CurrentAssignment;
		std::vector<sTechnique> Techniques;
	};

	template<typename Rule>
	struct action {};

	template<>
	struct action<one<'{'>>
	{
		template<typename Input>
		static void apply(const Input& in, state& s)
		{
			s.Depth++;

			if (s.Depth == 1) // enters technique
			{
				// CurrentTechnique already initialized in action<technique_name>
			}
			else if (s.Depth == 2) // enters pass
			{
				s.CurrentPass = sTechniquePass();
			}
		}
	};

	template<>
	struct action<one<'}'>>
	{
		template<typename Input>
		static void apply(const Input& in, state& s)
		{
			s.Depth--;

			if (s.Depth == 0) // exits technique
			{
				s.Techniques.push_back(s.CurrentTechnique);
			}
			else if (s.Depth == 1) // exits pass
			{
				s.CurrentTechnique.Passes.push_back(s.CurrentPass);
			}
		}
	};

	template<>
	struct action<technique_name>
	{
		template<typename Input>
		static void apply(const Input& in, state& s)
		{
			s.CurrentTechnique = sTechnique();
			s.CurrentTechnique.Name = in.string();
		}
	};

	template<>
	struct action<pass_assignment_name>
	{
		template<typename Input>
		static void apply(const Input& in, state& s)
		{
			s.CurrentAssignment = sAssigment();
			s.CurrentAssignment.Type = in.string();
		}
	};

	template<>
	struct action<pass_assignment_value>
	{
		template<typename Input>
		static void apply(const Input& in, state& s)
		{
			s.CurrentAssignment.Value = in.string();
			s.CurrentPass.Assigments.push_back(s.CurrentAssignment);
		}
	};
} // namespace technique

CEffect::CEffect(const std::string& source)
	: mSource(source)
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

void CEffect::EnsureTechniques()
{
	if (!mTechniques.empty())
	{
		return;
	}

	technique::state s;
	pegtl::string_input<> in(mSource, "CEffect");
	try
	{
		pegtl::parse<technique::grammar, technique::action>(in, s);
	}
	catch (const pegtl::parse_error& e)
	{
		auto& p = e.positions.front();
		std::cerr << e.what() << std::endl
			<< in.line_at(p) << std::endl
			<< std::string(p.byte_in_line, ' ') << '^' << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	mTechniques = s.Techniques;
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

static const char* GetTargetForProgram(eProgramType type)
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

std::unique_ptr<CCodeBlob> CEffect::CompileProgram(const std::string& entrypoint, eProgramType type) const
{
	// Flags used in the game shaders (except for D3DCOMPILE_NO_PRESHADER, which doesn't seem to be supported in our version of d3dcompile)
	constexpr uint32_t Flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;

	// TODO: implement ID3DInclude
	CComPtr<ID3DBlob> code, errorMsg;
	HRESULT r = D3DCompile(mSource.c_str(), mSource.size(), "CEffect", nullptr, nullptr, entrypoint.c_str(), GetTargetForProgram(type), Flags, 0, &code, &errorMsg);
	if (SUCCEEDED(r))
	{
		return std::make_unique<CCodeBlob>(code->GetBufferPointer(), static_cast<uint32_t>(code->GetBufferSize()));
	}
	else
	{
		throw std::exception(errorMsg ? reinterpret_cast<const char*>(errorMsg->GetBufferPointer()) : "Compilation error");
	}
}

static const char* GetAssignmentTypeForProgram(eProgramType type)
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

	const char* assignmentType = GetAssignmentTypeForProgram(type);

	for (auto& t : mTechniques)
	{
		for (auto& p : t.Passes)
		{
			for (auto& a : p.Assigments)
			{
				if (a.Type == assignmentType)
				{
					outEntrypoints.insert(a.Value);
				}
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

		std::string programName;
		const char* assignmentType = GetAssignmentTypeForProgram(type);
		for (auto& a : pass.Assigments)
		{
			if (a.Type == assignmentType)
			{
				programName = a.Value;
			}
		}

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
				throw std::exception("Program index too big");
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
