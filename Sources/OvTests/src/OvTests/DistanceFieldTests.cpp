/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <limits>

#include <doctest/doctest.h>

#include <OvCore/Helpers/DistanceField.h>
#include <OvTests/TestUtils.h>

using OvDebug::ELogLevel;

TEST_SUITE("Distance field")
{
	TEST_CASE("a shape that is not finite or too large is left out, with a warning")
	{
		OvTests::LogCapture logs;
		OvCore::Helpers::DistanceField field;
		const float nan = std::numeric_limits<float>::quiet_NaN();

		field.AddBox({ 0.0f, 0.0f, 0.0f }, { nan, 1.0f, 1.0f });
		field.AddTube({ 0.0f, 0.0f, 0.0f }, { 4.0f, 0.0f, 0.0f }, nan, 1.0f, 1.0f, 1.0f);
		field.AddPillar({ 0.0f, 0.0f, 0.0f }, std::numeric_limits<float>::infinity(), 1.0f);
		field.AddEllipsoid({ 0.0f, 0.0f, 0.0f }, { 1.0e7f, 1.0e7f, 1.0e7f }, 1.0f);
		field.AddLight({ nan, 0.0f, 0.0f }, 2.0f);
		CHECK(logs.Count(ELogLevel::LOG_WARNING) == 5);

		field.AddBox({ 0.0f, 0.0f, 0.0f }, { 4.0f, 2.0f, 4.0f });
		field.AddLight({ 1.0f, 1.0f, 1.0f }, 2.0f);
		CHECK(logs.Count(ELogLevel::LOG_WARNING) == 5);
	}
}
