#pragma once
#include <string_view>
#include "Effect.h"

class CEffectParser
{
private:
	std::string_view mSource;

public:
	// source: HLSL source code already preprocessed
	CEffectParser(std::string_view source);

	std::vector<sTechnique> GetTechniques() const;
	std::vector<sSamplerState> GetSamplerStates() const;
	std::vector<std::string> GetSharedVariablesNames() const;
};

