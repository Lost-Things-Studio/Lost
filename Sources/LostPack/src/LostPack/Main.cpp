/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <iostream>

#include <LostPack/Commands.h>

int main(int p_argc, char** p_argv)
{
	return LostPack::Run(std::vector<std::string>(p_argv + 1, p_argv + p_argc), std::cout, std::cerr);
}
