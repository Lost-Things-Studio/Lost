/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <stdexcept>

#include <doctest/doctest.h>

#include <OvTools/Eventing/Event.h>

TEST_SUITE("Events")
{
	TEST_CASE("listeners added or removed during a call take effect after it")
	{
		OvTools::Eventing::Event<> event;
		int first = 0;
		int added = 0;

		OvTools::Eventing::ListenerID firstID = 0;
		firstID = event += [&]
		{
			++first;
			event -= firstID;
			event += [&] { ++added; };
		};

		event.Invoke();
		CHECK(first == 1);
		CHECK(added == 0);

		event.Invoke();
		CHECK(first == 1);
		CHECK(added == 1);
	}

	TEST_CASE("an event stays usable after a listener throws")
	{
		OvTools::Eventing::Event<> event;
		const auto throwing = event += [] { throw std::runtime_error("listener failed"); };

		CHECK_THROWS_AS(event.Invoke(), std::runtime_error);

		// Out of the call, a change applies at once, and the next call runs what is listening then
		int calls = 0;
		CHECK(event.RemoveListener(throwing));
		event += [&] { ++calls; };

		CHECK_NOTHROW(event.Invoke());
		CHECK(calls == 1);
	}
}
