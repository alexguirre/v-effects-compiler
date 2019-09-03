#include "EffectSaver.h"
#include <assert.h>
#include <fstream>
#include <vector>
#include <d3dcompiler.h>
#include <atlbase.h>
#include <tuple>
#include <set>
#include "Effect.h"
#include "Hash.h"

namespace fs = std::filesystem;

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

	WriteHeader(f);
	WriteAnnotations(f);
	WritePrograms(f, eProgramType::Vertex);
	WritePrograms(f, eProgramType::Fragment);
	WritePrograms(f, eProgramType::Compute);
	WritePrograms(f, eProgramType::Domain);
	WritePrograms(f, eProgramType::Geometry);
	WritePrograms(f, eProgramType::Hull);
	WriteBuffers(f, true);
	WriteBuffers(f, false);
	WriteTechniques(f);

	// TODO: finish CEffectSaver::SaveTo
}

void CEffectSaver::WriteHeader(std::ostream& o) const
{
	constexpr uint32_t Header = ('r' << 0) | ('g' << 8) | ('x' << 16) | ('e' << 24);

	WriteUInt32(o, Header);
	WriteUInt32(o, 0xDEADBEEF); // TODO: vertex type
}

void CEffectSaver::WriteAnnotations(std::ostream& o) const
{
	WriteUInt8(o, 0); // no annotations support for now
}

struct sBufferDesc
{
	std::string Name;
	uint32_t Size = 0;
	uint32_t Register = 0;

	struct Comparer
	{
		bool operator()(const sBufferDesc& lhs, const sBufferDesc& rhs) const
		{
			return lhs.Name < rhs.Name;
		}
	};
};

struct sBufferVariableDesc
{
	std::string Name;
	uint32_t Offset = 0;
	uint32_t Count = 0;
	uint8_t Type = 0;
	uint32_t BufferNameHash = 0;

	struct Comparer
	{
		bool operator()(const sBufferVariableDesc& lhs, const sBufferVariableDesc& rhs) const
		{
			return lhs.Name < rhs.Name;
		}
	};
};

static uint8_t VarTypeD3D11ToRage(ID3D11ShaderReflectionType* type)
{
	enum grcEffectVarType : uint8_t
	{
		float_ = 2,
		float2 = 3,
		float3 = 4,
		float4 = 5,
		texture = 6,
		bool_ = 7,
		float3x4 = 8,
		float4x4 = 9,
		string = 10,
		int_ = 11,
		int2 = 12,
		int3 = 13,
		int4 = 14,
	};

	D3D11_SHADER_TYPE_DESC typeDesc;
	type->GetDesc(&typeDesc);

	switch (typeDesc.Type)
	{
	case D3D_SVT_FLOAT:
	{
		if (typeDesc.Class == D3D_SVC_MATRIX_ROWS || typeDesc.Class == D3D_SVC_MATRIX_COLUMNS)
		{
			if (typeDesc.Rows == 3 && typeDesc.Columns == 4)
			{
				return grcEffectVarType::float3x4;
			}
			else if (typeDesc.Rows == 4 && typeDesc.Columns == 4)
			{
				return grcEffectVarType::float4x4;
			}
		}
		else
		{
			switch (typeDesc.Columns)
			{
			case 1: return grcEffectVarType::float_;
			case 2: return grcEffectVarType::float2;
			case 3: return grcEffectVarType::float3;
			case 4: return grcEffectVarType::float4;
			}
		}
		break;
	}
	case D3D_SVT_INT:
	case D3D_SVT_UINT:
	{
		switch (typeDesc.Columns)
		{
		case 1: return grcEffectVarType::int_;
		case 2: return grcEffectVarType::int2;
		case 3: return grcEffectVarType::int3;
		case 4: return grcEffectVarType::int4;
		}
		break;
	}
	case D3D_SVT_STRING:
		return grcEffectVarType::string;
	case D3D_SVT_BOOL:
		return grcEffectVarType::bool_;
	case D3D_SVT_TEXTURE:
	case D3D_SVT_TEXTURE1D:
	case D3D_SVT_TEXTURE2D:
	case D3D_SVT_TEXTURE3D:
	case D3D_SVT_TEXTURECUBE:
	case D3D_SVT_SAMPLER:
	case D3D_SVT_SAMPLER1D:
	case D3D_SVT_SAMPLER2D:
	case D3D_SVT_SAMPLER3D:
	case D3D_SVT_SAMPLERCUBE:
		return grcEffectVarType::texture;
	}

	// TODO: support more variable types
	throw std::runtime_error("Unsupported variable type");
}

static void GetBuffersDesc(const CCodeBlob& code, std::set<sBufferDesc, sBufferDesc::Comparer>& outBuffers, std::set<sBufferVariableDesc, sBufferVariableDesc::Comparer>& outBufferVars, bool globals, bool locals)
{
	static std::set<std::string_view> knownGlobalBuffers =
	{
		"rage_clipplanes", "rage_matrices", "misc_global", "lighting_globals", "rage_bonemtx", "rage_cbinst_matrices", "rage_cbinst_update"
		// TODO: there's some more global buffers
	};

	CComPtr<ID3D11ShaderReflection> reflection;
	HRESULT r = D3DReflect(code.Data(), code.Size(), __uuidof(ID3D11ShaderReflection), reinterpret_cast<void**>(&reflection));
	if (FAILED(r))
	{
		return;
	}

	D3D11_SHADER_DESC shaderDesc;
	if (FAILED(reflection->GetDesc(&shaderDesc)))
	{
		return;
	}

	for (uint32_t i = 0; i < shaderDesc.ConstantBuffers; i++)
	{
		ID3D11ShaderReflectionConstantBuffer* buffer = reflection->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		bool isGlobalBuffer = knownGlobalBuffers.find(bufferDesc.Name) != knownGlobalBuffers.end();
		if ((globals && isGlobalBuffer) || (!globals && !isGlobalBuffer) ||
			(locals && !isGlobalBuffer) || (!locals && isGlobalBuffer))
		{
			D3D11_SHADER_INPUT_BIND_DESC bindDesc;
			reflection->GetResourceBindingDescByName(bufferDesc.Name, &bindDesc);

			sBufferDesc d;
			d.Name = bufferDesc.Name;
			d.Size = bufferDesc.Size;
			d.Register = bindDesc.BindPoint;

			outBuffers.insert(d);

			for (uint32_t j = 0; j < bufferDesc.Variables; j++)
			{
				ID3D11ShaderReflectionVariable* var = buffer->GetVariableByIndex(j);
				D3D11_SHADER_VARIABLE_DESC varDesc;
				var->GetDesc(&varDesc);

				ID3D11ShaderReflectionType* varType = var->GetType();
				D3D11_SHADER_TYPE_DESC varTypeDesc;
				varType->GetDesc(&varTypeDesc);

				// TODO: buffer variables require more data for WriteBuffers
				sBufferVariableDesc v;
				v.Name = varDesc.Name;
				v.Offset = varDesc.StartOffset;
				v.Count = varTypeDesc.Elements;
				v.Type = VarTypeD3D11ToRage(varType);
				v.BufferNameHash = joaat(bufferDesc.Name);

				outBufferVars.insert(v);
			}
		}
	}
}

void CEffectSaver::WritePrograms(std::ostream& o, eProgramType type) const
{
	if (type == eProgramType::Vertex || type == eProgramType::Fragment)
	{
		std::set<std::string> entrypoints;
		mEffect.GetUsedPrograms(entrypoints, type);

		if (entrypoints.size() > std::numeric_limits<uint8_t>::max() - 1) // -1 to account for implicit NULL program
		{
			throw std::length_error("Number of programs exceeds " + std::to_string(std::numeric_limits<uint8_t>::max()));
		}

		WriteUInt8(o, static_cast<uint8_t>(entrypoints.size() + 1)); // program count
		
		WriteNullProgram(o, type);

		for (const auto& e : entrypoints)
		{
			const CCodeBlob& code = mEffect.GetProgramCode(e);
			std::set<sBufferDesc, sBufferDesc::Comparer> buffers;
			std::set<sBufferVariableDesc, sBufferVariableDesc::Comparer> bufferVars;
			GetBuffersDesc(code, buffers, bufferVars, true, true);
			
			if (buffers.size() > std::numeric_limits<uint8_t>::max())
			{
				throw std::length_error("Number of buffers exceeds " + std::to_string(std::numeric_limits<uint8_t>::max()));
			}

			if (bufferVars.size() > std::numeric_limits<uint8_t>::max())
			{
				throw std::length_error("Number of buffer variables exceeds " + std::to_string(std::numeric_limits<uint8_t>::max()));
			}

			WriteLengthPrefixedString(o, e);

			// buffers variables
			WriteUInt8(o, static_cast<uint8_t>(bufferVars.size())); // var count
			for (const auto& v : bufferVars)
			{
				WriteLengthPrefixedString(o, v.Name); // var name
			}

			// buffers
			WriteUInt8(o, static_cast<uint8_t>(buffers.size())); // buffer count
			for (const auto& b : buffers)
			{
				WriteLengthPrefixedString(o, b.Name); // buffer name
				WriteUInt8(o, static_cast<uint8_t>(b.Register)); // register
				WriteUInt8(o, 0); // what does this byte mean?
			}

			// bytecode
			WriteUInt32(o, code.Size());
			if (code.Size() > 0)
			{
				o.write(reinterpret_cast<const char*>(code.Data()), code.Size());
				WriteUInt8(o, 4); // target version major
				WriteUInt8(o, 0); // target version minor
			}
		}
	}
	else
	{
		// TODO: WritePrograms for programs other than vertex/fragment
		WriteUInt8(o, 1); // program count

		WriteNullProgram(o, type);
	}
}

void CEffectSaver::WriteNullProgram(std::ostream& o, eProgramType type) const
{
	WriteLengthPrefixedString(o, CEffect::NullProgramName);
	WriteUInt8(o, 0); // buffer variable count
	WriteUInt8(o, 0); // buffer count
	if (type == eProgramType::Geometry)
	{
		WriteUInt8(o, 0); // unk count
	}
	WriteUInt32(o, 0); // bytecode size
}

void CEffectSaver::WriteBuffers(std::ostream& o, bool globals) const
{
	std::set<sBufferDesc, sBufferDesc::Comparer> buffers;
	std::set<sBufferVariableDesc, sBufferVariableDesc::Comparer> bufferVars;
	for (int i = 0; i < static_cast<int>(eProgramType::NumberOfTypes); i++)
	{
		std::set<std::string> programs;
		mEffect.GetUsedPrograms(programs, static_cast<eProgramType>(i));

		for (const auto& p : programs)
		{
			const CCodeBlob& code = mEffect.GetProgramCode(p);
			GetBuffersDesc(code, buffers, bufferVars, globals, !globals);
		}
	}

	if (buffers.size() > std::numeric_limits<uint8_t>::max())
	{
		throw std::length_error("Number of buffers exceeds " + std::to_string(std::numeric_limits<uint8_t>::max()));
	}

	if (bufferVars.size() > std::numeric_limits<uint8_t>::max())
	{
		throw std::length_error("Number of buffer variables exceeds " + std::to_string(std::numeric_limits<uint8_t>::max()));
	}

	// buffers
	WriteUInt8(o, static_cast<uint8_t>(buffers.size()));
	for (auto& b : buffers)
	{
		WriteUInt32(o, b.Size);
		for (int i = 0; i < static_cast<int>(eProgramType::NumberOfTypes); i++)
		{
			WriteUInt16(o, static_cast<uint16_t>(b.Register));
		}
		WriteLengthPrefixedString(o, b.Name);
	}

	// TODO: variables not from cbuffers, like texture or samplers
	WriteUInt8(o, static_cast<uint8_t>(bufferVars.size()));
	for (auto& v : bufferVars)
	{
		WriteUInt8(o, v.Type); // type
		WriteUInt8(o, static_cast<uint8_t>(v.Count)); // count
		WriteUInt8(o, 0); // flags 1, TODO: variable flags
		WriteUInt8(o, 0); // flags 2
		WriteLengthPrefixedString(o, v.Name); // name
		WriteLengthPrefixedString(o, v.Name); // description
		WriteUInt32(o, v.Offset); // offset
		WriteUInt32(o, v.BufferNameHash); // buffer name hash
		WriteUInt8(o, 0); // annotation count, no annotations support for now
		WriteUInt8(o, 0); // initial values count, TODO: variable initial values
	}
}

void CEffectSaver::WriteTechniques(std::ostream& o) const
{
	if (mEffect.Techniques().size() > std::numeric_limits<uint8_t>::max())
	{
		throw std::length_error("Number of techniques exceeds " + std::to_string(std::numeric_limits<uint8_t>::max()));
	}

	WriteUInt8(o, static_cast<uint8_t>(mEffect.Techniques().size()));

	for (auto& t : mEffect.Techniques())
	{
		WriteLengthPrefixedString(o, t.Name);

		if (t.Passes.size() > std::numeric_limits<uint8_t>::max())
		{
			throw std::length_error("Number of passes exceeds " + std::to_string(std::numeric_limits<uint8_t>::max()));
		}

		WriteUInt8(o, static_cast<uint8_t>(t.Passes.size())); // pass count
		for (auto& p : t.Passes)
		{
			uint8_t programs[static_cast<size_t>(eProgramType::NumberOfTypes)];
			mEffect.GetPassPrograms(p, programs);
			for (uint8_t idx : programs)
			{
				WriteUInt8(o, idx);
			}

			WriteUInt8(o, static_cast<uint8_t>(p.Assigments.size())); // assignment count
			for (auto& a : p.Assigments)
			{
				WriteUInt32(o, static_cast<uint32_t>(a.Type));
				WriteUInt32(o, a.Value);
			}
		}
	}
}

void CEffectSaver::WriteLengthPrefixedString(std::ostream& o, const std::string& str) const
{
	size_t length = str.size() + 1; // + null terminator
	if (length > std::numeric_limits<uint8_t>::max())
	{
		throw std::length_error("String length exceeds " + std::to_string(std::numeric_limits<uint8_t>::max()) + " characters");
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
