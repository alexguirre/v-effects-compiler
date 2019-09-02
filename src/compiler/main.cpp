#include <iostream>
#include <string>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <tclap/CmdLine.h>
#include "Effect.h"
#include "EffectSaver.h"

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
	TCLAP::CmdLine cmd("Shader effect compiler for Grand Theft Auto V", ' ', "WIP");
	TCLAP::UnlabeledValueArg<std::string> inputArg("input", "Specifies the filename of the input file.", true, "", "input_file");
	TCLAP::ValueArg<std::string> outputArg("o", "output", "Specifies the filename of the output file.", false, "", "file");
	TCLAP::SwitchArg preprocessArg("p", "preprocess", "Preprocesses the input file instead of compiling it.", false);

	cmd.add(inputArg);
	cmd.add(outputArg);
	cmd.add(preprocessArg);

	cmd.parse(argc, argv);

	fs::path inputPath = fs::absolute(inputArg.getValue());
	
	if (!fs::exists(inputPath))
	{
		throw std::runtime_error("Path '" + inputPath.string() + "' does not exist");
	}

	if (!fs::is_regular_file(inputPath))
	{
		throw std::runtime_error("Path '" + inputPath.string() + "' does not refer to a file");
	}

	fs::path outputPath = inputPath;
	if (outputArg.isSet())
	{
		outputPath = fs::absolute(outputArg.getValue());
	}
	else if (preprocessArg.getValue())
	{
		outputPath.replace_filename("preprocessed." + outputPath.filename().string());
	}
	else
	{
		outputPath.replace_extension("fxc");
	}
	
	std::ifstream inputFile(inputPath);
	std::stringstream srcBuffer;
	srcBuffer << inputFile.rdbuf();

	std::string src = srcBuffer.str();
	std::unique_ptr<CEffect> fx = std::make_unique<CEffect>(src);

	if (preprocessArg.getValue())
	{
		std::ofstream outputStream(outputPath, std::ios::trunc);
		outputStream << fx->PreprocessSource();
	}
	else
	{
		CEffectSaver saver(*fx);
		saver.SaveTo(outputPath);
	}

	return 0;
}
