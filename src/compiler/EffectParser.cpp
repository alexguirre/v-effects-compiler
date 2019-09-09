#include "EffectParser.h"
#include "HlslGrammar.h"

CEffectParser::CEffectParser(std::string_view source)
	: mSource(source)
{
}

std::vector<sTechnique> CEffectParser::GetTechniques() const
{
	hlsl_grammar::technique_state s;
	pegtl::string_input<> in(mSource, "CEffectParser");
	try
	{
		pegtl::parse<hlsl_grammar::technique_grammar, hlsl_grammar::technique_action>(in, s);

		return s.Techniques;
	}
	catch (const pegtl::parse_error& e)
	{
		auto& p = e.positions.front();

		throw std::runtime_error(
			"Technique parser error:\n" +
			std::string(e.what()) + "\n" +
			in.line_at(p) + "\n" +
			std::string(p.byte_in_line, ' ') + "^\n"
		);
	}
}
