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
	try
	{
		TCLAP::CmdLine cmd("Shader effect compiler for Grand Theft Auto V", ' ', "WIP");
		TCLAP::UnlabeledValueArg<std::filesystem::path> inputArg("input_file", "Specifies the filename of the input file.", true, "", "input_file");
		TCLAP::ValueArg<std::filesystem::path> outputArg("o", "output", "Specifies the filename of the output file.", false, "", "file");
		TCLAP::MultiArg<std::filesystem::path> includeDirsArg("i", "include_directories", "Specifies additional include directories.", false, "directory");
		TCLAP::SwitchArg preprocessArg("p", "preprocess", "Preprocesses the input file instead of compiling it.", false);

		cmd.add(inputArg);
		cmd.add(outputArg);
		cmd.add(includeDirsArg);
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
		const auto& includeDirs = includeDirsArg.getValue();		
		std::unique_ptr<CEffect> fx = std::make_unique<CEffect>(src, inputPath, includeDirs);

		for (auto& s : fx->SamplerStates())
		{
			std::cout << "SamplerState '" << s.Name << "'\n";
			for (auto& a : s.Assignments)
			{
				std::cout << "\t" << sAssignment::TypeToName.at(a.Type) << " = " << std::to_string(a.Value) << "\n";
			}
		}

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

		return EXIT_SUCCESS;
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << std::endl;

		return EXIT_FAILURE;
	}
}
