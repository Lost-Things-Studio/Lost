/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#include <doctest/doctest.h>

#include <OvTests/TestUtils.h>

TEST_SUITE("UI library")
{
	TEST_CASE("the UI library reads the input it consumes")
	{
		// The controller consumes what lands on a panel, so a panel that read the mouse or the keyboard as gameplay does
		// would see it released at once: a button would lose every click
		const auto library = OvTests::FindEngineResources() / "Libraries" / "UI";
		REQUIRE(std::filesystem::is_directory(library));

		const std::regex read(R"(Inputs\.(GetMouseButton|GetMouseButtonDown|GetMouseButtonUp|GetKey|GetKeyDown|GetKeyUp|GetMouseScroll|GetTypedText|GetKeyRepeatCount)\(([^()]*)\))");
		size_t reads = 0;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(library))
		{
			if (entry.path().extension() != ".lua")
				continue;

			std::ifstream file(entry.path());
			std::string line;
			int number = 0;

			while (std::getline(file, line))
			{
				++number;

				if (const auto comment = line.find("--"); comment != std::string::npos)
					line.erase(comment);

				for (auto it = std::sregex_iterator(line.begin(), line.end(), read); it != std::sregex_iterator(); ++it)
				{
					++reads;
					const std::string arguments = (*it)[2].str();
					const bool includesConsumed = arguments == "true" || arguments.ends_with(", true");
					CHECK_MESSAGE(includesConsumed, entry.path().filename().string() << ":" << number << ": " << (*it)[0].str());
				}
			}
		}

		CHECK(reads > 10);
	}
}
