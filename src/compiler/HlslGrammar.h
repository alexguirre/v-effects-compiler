#pragma once
#include <tao/pegtl.hpp>
#include <tao/pegtl/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>
#include "Effect.h"

namespace pegtl = tao::TAO_PEGTL_NAMESPACE;

namespace hlsl_grammar
{
	using namespace pegtl;

	struct str_technique : TAO_PEGTL_STRING("technique") {};
	struct str_pass : TAO_PEGTL_STRING("pass") {};
	struct str_shared : TAO_PEGTL_STRING("shared") {};
	struct str_const : TAO_PEGTL_STRING("const") {};
	struct str_row_major : TAO_PEGTL_STRING("row_major") {};
	struct str_column_major : TAO_PEGTL_STRING("column_major") {};
	struct str_sampler : TAO_PEGTL_STRING("sampler") {};
	struct str_sampler1D : TAO_PEGTL_STRING("sampler1D") {};
	struct str_sampler2D : TAO_PEGTL_STRING("sampler2D") {};
	struct str_sampler3D : TAO_PEGTL_STRING("sampler3D") {};
	struct str_samplerCUBE : TAO_PEGTL_STRING("samplerCUBE") {};
	struct str_SamplerState : TAO_PEGTL_STRING("SamplerState") {};
	struct str_register : TAO_PEGTL_STRING("register") {};

	// this rule doesn't support multi-line directives but should be good enough for our case since we parse
	// the techniques on the preprocessed source code and the only directives should be `#line` directives
	struct preprocessor_directive : seq<bol, star<space>, one<'#'>, until<eolf>> {};
	struct comment : seq<two<'/'>, until<eolf>> {};
	struct comment_multiline : seq<one<'/'>, one<'*'>, until<seq<one<'*'>, one<'/'>>>> {};
	struct sp : sor<space, comment, comment_multiline, preprocessor_directive> {};
	struct sp_s : star<sp> {};
	struct sp_p : plus<sp> {};

	struct integer_dec : plus<digit> {};
	struct integer_hex : seq<one<'0'>, one<'x'>, plus<xdigit>> {};
	struct integer : sor<integer_hex, integer_dec> {};

	template<typename Rule, char C1, char C2>
	struct surrounded : if_must<one<C1>, sp_s, Rule, sp_s, one<C2>> {};
	template<typename Rule>
	struct braces : surrounded<Rule, '{', '}'> {};
	template<typename Rule>
	struct parens : surrounded<Rule, '(', ')'> {};

	struct assignment_name : identifier {};
	struct assignment_value_string : identifier {};
	struct assignment_value_integer : integer {};
	struct assignment_value : sor<assignment_value_integer, assignment_value_string> {};
	struct assignment : seq<
		assignment_name, sp_s,
		one<'='>, sp_s,
		assignment_value, sp_s,
		one<';'>, sp_s
	> {};
	
	struct pass : seq<str_pass, sp_s, braces<star<assignment>>, sp_s> {};

	struct technique_name : identifier {};
	struct technique : seq<str_technique, sp_p, technique_name, sp_s, braces<star<pass>>, sp_s> {};

	struct technique_grammar : star<until<technique>> {};

	struct technique_state
	{
		struct sRawAssignment
		{
			std::string Type;
			std::string Value;
		};

		int Depth = 0;
		sTechnique CurrentTechnique;
		sTechniquePass CurrentPass;
		sRawAssignment CurrentAssignment;
		std::vector<sTechnique> Techniques;
	};

	template<typename Rule>
	struct technique_action {};

	template<>
	struct technique_action<one<'{'>>
	{
		template<typename Input>
		static void apply(const Input& /* in */, technique_state& s)
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
	struct technique_action<one<'}'>>
	{
		template<typename Input>
		static void apply(const Input& /* in */, technique_state& s)
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
	struct technique_action<technique_name>
	{
		template<typename Input>
		static void apply(const Input& in, technique_state& s)
		{
			s.CurrentTechnique = sTechnique();
			s.CurrentTechnique.Name = in.string();
		}
	};

	template<>
	struct technique_action<assignment_name>
	{
		template<typename Input>
		static void apply(const Input& in, technique_state& s)
		{
			s.CurrentAssignment = technique_state::sRawAssignment();
			s.CurrentAssignment.Type = in.string();
		}
	};

	template<>
	struct technique_action<assignment_value>
	{
		template<typename Input>
		static void apply(const Input& in, technique_state& s)
		{
			s.CurrentAssignment.Value = in.string();

			bool isShaderAssignment = false;
			for (int i = 0; i < static_cast<int>(eProgramType::NumberOfTypes); i++)
			{
				eProgramType type = static_cast<eProgramType>(i);
				if (s.CurrentAssignment.Type == CEffect::GetAssignmentTypeForProgram(type))
				{
					s.CurrentPass.Shaders[i] = s.CurrentAssignment.Value;
					isShaderAssignment = true;
					break;
				}
			}

			if (!isShaderAssignment)
			{
				s.CurrentPass.Assignments.push_back(sAssignment::GetTechniquePassAssignment(s.CurrentAssignment.Type, s.CurrentAssignment.Value));
			}
		}
	};


	struct shared_variable_storage_class : str_shared {};
	struct shared_variable_type_modifier : sor<
		str_const, str_row_major, str_column_major
	> {};
	struct shared_variable_type : identifier {};
	struct shared_variable_name : identifier {};
	struct shared_variable : seq<
		shared_variable_storage_class, sp_p,
		opt<shared_variable_type_modifier>, sp_s,
		shared_variable_type, sp_s,
		shared_variable_name, sp_s
	> {};

	struct shared_variable_grammar : star<until<shared_variable>> {};

	struct shared_variable_state
	{
		std::vector<std::string> Names;
	};

	template<typename Rule>
	struct shared_variable_action {};

	template<>
	struct shared_variable_action<shared_variable_name>
	{
		template<typename Input>
		static void apply(const Input& in, shared_variable_state& s)
		{
			s.Names.push_back(in.string());
		}
	};


	struct sampler_type : sor<
		str_sampler, str_sampler1D, str_sampler2D,
		str_sampler3D, str_samplerCUBE, str_SamplerState
	> {};
	struct sampler_name : identifier {};
	struct sampler_register : seq<
		one<':'>, sp_s,
		str_register, sp_s,
		parens<
			seq<alpha, integer_dec>
		>, sp_s
	> {};
	struct sampler_assignments : seq<
		braces<star<assignment>>
	> {};
	struct sampler : seq<
		sampler_type, sp_s,
		sampler_name, sp_s,
		star<sampler_register>, sp_s,
		opt<sampler_assignments>, sp_s,
		one<';'>
	> {};

	struct sampler_grammar : star<until<sampler>> {};

	struct sampler_state
	{
		struct sRawAssignment
		{
			std::string Type;
			std::string Value;
		};

		int Depth = 0;
		sSamplerState CurrentSampler;
		sRawAssignment CurrentAssignment;
		std::vector<sSamplerState> Samplers;
	};

	template<typename Rule>
	struct sampler_action {};

	template<>
	struct sampler_action<one<'{'>>
	{
		template<typename Input>
		static void apply(const Input& /* in */, sampler_state& s)
		{
			s.Depth++;

			if (s.Depth == 1) // enters sampler state assignments
			{
				// nothing to do
			}
		}
	};

	template<>
	struct sampler_action<one<'}'>>
	{
		template<typename Input>
		static void apply(const Input& /* in */, sampler_state& s)
		{
			s.Depth--;

			if (s.Depth == 0) // exits sampler state assignments
			{
				s.Samplers.push_back(s.CurrentSampler);
			}
		}
	};

	template<>
	struct sampler_action<sampler_name>
	{
		template<typename Input>
		static void apply(const Input& in, sampler_state& s)
		{
			s.CurrentSampler = sSamplerState();
			s.CurrentSampler.Name = in.string();
		}
	};

	template<>
	struct sampler_action<assignment_name>
	{
		template<typename Input>
		static void apply(const Input& in, sampler_state& s)
		{
			s.CurrentAssignment = sampler_state::sRawAssignment();
			s.CurrentAssignment.Type = in.string();
		}
	};

	template<>
	struct sampler_action<assignment_value>
	{
		template<typename Input>
		static void apply(const Input& in, sampler_state& s)
		{
			s.CurrentAssignment.Value = in.string();

			s.CurrentSampler.Assignments.push_back(sAssignment::GetSamplerStateAssignment(s.CurrentAssignment.Type, s.CurrentAssignment.Value));
		}
	};
} // namespace hlsl_grammar