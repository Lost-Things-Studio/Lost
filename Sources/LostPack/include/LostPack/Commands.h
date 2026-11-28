/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace LostPack
{
	/**
	* What LostPack ends with
	*/
	enum EExitCode : int
	{
		SUCCESS = 0,
		FAILURE = 1, // The package, or a file, is at fault
		USAGE = 2 // The command line is
	};

	/**
	* How to call LostPack
	*/
	std::string Usage();

	/**
	* Runs one command (see Docs/Packages.md):
	* - pack <folder> <package> [--project-id <id>]
	* - list <package>
	* - verify <package>
	* - extract <package> <folder>
	* @param p_arguments (without the program's name)
	* @param p_out (what a command reports)
	* @param p_err (what went wrong, and the usage)
	*/
	int Run(const std::vector<std::string>& p_arguments, std::ostream& p_out, std::ostream& p_err);
}
