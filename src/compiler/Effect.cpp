#include "Effect.h"
#include <tao/pegtl.hpp>
#include <tao/pegtl/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>

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