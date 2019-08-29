#include "EffectSaver.h"
#include <assert.h>
#include <fstream>
#include <vector>
#include <d3dcompiler.h>
#include <atlbase.h>
#include <tuple>
#include "Effect.h"

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

static void GetProgramBuffers(const CCodeBlob& code, std::vector<std::tuple<std::string, int>>& outBuffersAndRegisters, std::vector<std::string>& outBufferVariables)
{
	outBuffersAndRegisters.clear();
	outBufferVariables.clear();

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

	for (int i = 0; i < shaderDesc.ConstantBuffers; i++)
	{
		ID3D11ShaderReflectionConstantBuffer* buffer = reflection->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		for (int j = 0; j < bufferDesc.Variables; j++)
		{
			ID3D11ShaderReflectionVariable* var = buffer->GetVariableByIndex(j);
			D3D11_SHADER_VARIABLE_DESC varDesc;
			var->GetDesc(&varDesc);

			outBufferVariables.push_back(varDesc.Name);
		}

		D3D11_SHADER_INPUT_BIND_DESC bindDesc;
		reflection->GetResourceBindingDescByName(bufferDesc.Name, &bindDesc);

		outBuffersAndRegisters.push_back(std::make_tuple(bindDesc.Name, bindDesc.BindPoint));
	}
}

void CEffectSaver::WritePrograms(std::ostream& o, eProgramType type) const
{
	if (type == eProgramType::Vertex || type == eProgramType::Fragment)
	{
		std::vector<std::string> entrypoints;
		mEffect.GetUsedPrograms(entrypoints, type);

		if (entrypoints.size() > 254)
		{
			throw std::exception("Too many programs");
		}

		WriteUInt8(o, static_cast<uint8_t>(entrypoints.size() + 1)); // program count

		// NULL program
		{
			WriteLengthPrefixedString(o, "NULL");
			WriteUInt8(o, 0); // buffer variable count
			WriteUInt8(o, 0); // buffer count
			WriteUInt32(o, 0); // bytecode size
		}

		for (const auto& e : entrypoints)
		{
			std::unique_ptr<CCodeBlob> code = mEffect.CompileProgram(e, type);
			std::vector<std::tuple<std::string, int>> buffersAndRegisters;
			std::vector<std::string> bufferVariables;
			GetProgramBuffers(*code, buffersAndRegisters, bufferVariables);
			
			if (buffersAndRegisters.size() > 255)
			{
				throw std::exception("Too many buffers");
			}

			if (bufferVariables.size() > 255)
			{
				throw std::exception("Too many buffer variables");
			}

			WriteLengthPrefixedString(o, e);

			// buffers variables
			WriteUInt8(o, static_cast<uint8_t>(bufferVariables.size())); // var count
			for (const auto& v : bufferVariables)
			{
				WriteLengthPrefixedString(o, v); // var name
			}

			// buffers
			WriteUInt8(o, static_cast<uint8_t>(buffersAndRegisters.size())); // buffer count
			for (const auto& b : buffersAndRegisters)
			{
				WriteLengthPrefixedString(o, std::get<0>(b)); // buffer name
				WriteUInt8(o, static_cast<uint8_t>(std::get<1>(b))); // register
				WriteUInt8(o, 0); // what does this byte mean?
			}

			// bytecode
			WriteUInt32(o, code->Size());
			if (code->Size() > 0)
			{
				o.write(reinterpret_cast<const char*>(code->Data()), code->Size());
				WriteUInt8(o, 4); // target version major
				WriteUInt8(o, 0); // target version minor
			}
		}
	}
	else
	{
		// TODO: WritePrograms for programs other than vertex/fragment
		WriteUInt8(o, 1); // program count

		// NULL program
		{
			WriteLengthPrefixedString(o, "NULL");
			WriteUInt8(o, 0); // buffer variable count
			WriteUInt8(o, 0); // buffer count
			WriteUInt32(o, 0); // bytecode size
		}
	}
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
