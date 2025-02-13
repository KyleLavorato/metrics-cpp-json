#include <metrics/registry.h>
#include <metrics/serialize.h>
#include <metrics/timer.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <map>
#include <thread>
#include <chrono>

using namespace Metrics;
using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;
using std::this_thread::sleep_for;
using Catch::Matchers::Equals;

TEST_CASE("Metric.Labels", "[metric][labels]")
{
    auto l1 = Labels{ {"a", "b"}, {"c", "d"} };
    auto l2 = Labels{ {"c", "d"}, {"a", "b"} };
    auto l3 = Labels{ {"a", "b"}, {"c", "e"} };

    REQUIRE((l1 == l2));
    REQUIRE((l1 != l3));
    REQUIRE((l1 < l3));

    l3["c"] = "d";
    REQUIRE((l1 == l3));

    REQUIRE((Labels{ {"a", "a1"}, { "a", "a2" } } == Labels{ {"a", "a1"} }));
}

TEST_CASE("Metric.Counter", "[metric][counter]")
{
    Counter counter;
    REQUIRE(counter == 0);
    counter++;
    REQUIRE(counter == 1);

    auto counter2 = counter;
    auto counter3(counter);
    auto counter4{ counter };

    counter += 99;
    REQUIRE(counter == 100);
    REQUIRE(counter2 == 100);
    REQUIRE(counter3 == 100);
    REQUIRE(counter4 == 100);

    counter.reset();
    REQUIRE(counter == 0);
    REQUIRE(counter2 == 0);
    REQUIRE(counter3 == 0);
    REQUIRE(counter4 == 0);
}

TEST_CASE("Metric.Gauge", "[metric][gauge]")
{
    Gauge gauge;
    REQUIRE(gauge == 0);
    gauge = 5.0;

    auto gauge2 = gauge;
    auto gauge3(gauge);
    auto gauge4{ gauge };

    REQUIRE(gauge == 5.0);
    REQUIRE(gauge2 == 5.0);
    REQUIRE(gauge3 == 5.0);
    REQUIRE(gauge4 == 5.0);

    gauge += 3.0;
    REQUIRE(gauge == 8.0);
    gauge -= 5.0;
    REQUIRE(gauge == 3.0);
}

TEST_CASE("Metric.Histogram", "[metric][histogram]")
{
    Histogram histogram({ 1., 2., 5. });
    histogram.observe(1);
    histogram.observe(2);
    histogram.observe(3);
    histogram.observe(7);
    REQUIRE(histogram.sum() == 13);
    REQUIRE(histogram.count() == 4);

    auto values = histogram.values();

    REQUIRE(values[0].first == 1.);
    REQUIRE(values[1].first == 2.);
    REQUIRE(values[2].first == 5.);
    REQUIRE(values[0].second == 1);
    REQUIRE(values[1].second == 2);
    REQUIRE(values[2].second == 3);
}

TEST_CASE("Metric.Summary", "[metric][summary]")
{
    Summary summary({ .5, .9, .99 });
    summary.observe(1);
    summary.observe(2);
    summary.observe(3);
    summary.observe(5);
    REQUIRE(summary.sum() == 11);
    REQUIRE(summary.count() == 4);
}

std::unique_ptr<IRegistry> createReferenceRegistry()
{
    auto registry = createRegistry();
    registry->getCounter({ "counter1" }) += 1;
    registry->getCounter({ "counter2", { { "some", "label" } } }) += 2;
    registry->getGauge({ "gauge1" }) = 100.;
    registry->getGauge({ "gauge2", {{ "another", "label" }} }) = 200.;

    registry->getHistogram({ "histogram1" }, { 1., 2., 5. }).observe(1).observe(2);
    registry->getHistogram({ "histogram2", {{"more", "labels"}} }, { 1., 2., 5. }).observe(3).observe(4);

    return registry;
}

TEST_CASE("Registry.Registry", "[registry]")
{
    auto registry = createReferenceRegistry();

    CHECK_THROWS(registry->getGauge({ "counter1" }));
    CHECK_THROWS(registry->getCounter({ "gauge1" }));
    CHECK_THROWS(registry->getHistogram({ "counter1" }));

    auto contains = [](vector<Key> container, Key key)
    {
        for (const auto& k : container)
            if (k == key)
                return true;
        return false;
    };

    auto keys = registry->keys();

    REQUIRE(contains(keys, { "counter1" }));
    REQUIRE(contains(keys, { "counter2", { { "some", "label" } } }));
    REQUIRE(contains(keys, { "gauge1" }));
    REQUIRE(contains(keys, { "gauge2", {{ "another", "label" }} }));
};

TEST_CASE("Serialize.Prometheus", "[prometheus]")
{
    auto registry = createReferenceRegistry();
    auto result = serializePrometheus(*registry);

    REQUIRE_THAT(result, Equals(R"(counter1 1
counter2{some="label"} 2
gauge1 100
gauge2{another="label"} 200
# TYPE histogram1 histogram
histogram1{le="1"} 1
histogram1{le="2"} 2
histogram1{le="5"} 2
histogram1_sum 3
histogram1_count 2
# TYPE histogram2 histogram
histogram2{more="labels",le="1"} 0
histogram2{more="labels",le="2"} 0
histogram2{more="labels",le="5"} 2
histogram2_sum{more="labels"} 7
histogram2_count{more="labels"} 2
)"));
}

TEST_CASE("Serialize.Statsd", "[statsd]")
{
    auto registry = createReferenceRegistry();
    auto result = serializeStatsd(*registry);

    REQUIRE_THAT(result, Equals(R"(counter1|1|c
counter2,some=label|2|c
gauge1|100|g
gauge2,another=label|200|g
)"));
}

TEST_CASE("Serialize.JSON", "[JSON]")
{
    auto registry = createReferenceRegistry();
    std::string result = serializeJSON(*registry, "");
    REQUIRE_THAT(result, Equals(R"({"counter1":1,"counter2":2,"gauge1":100.0,"gauge2":200.0,"histogram1":{"1x":1,"2x":2,"5x":2,"sum":3.0,"average":1.5,"count":2},"histogram2":{"1x":0,"2x":0,"5x":2,"sum":7.0,"average":3.5,"count":2}})"));

    std::string empty = serializeJSON(*registry, "test.json");
    CHECK(empty.length() == 0);

    FILE* fp = fopen("test.json", "r");
    if (fp)
    {
        char buffer[512] = { 0 };
        fread(buffer, result.length(), 1, fp);
        std::string json(buffer);
        fclose(fp);

        REQUIRE_THAT(result, Equals(json));
    }
    else
    {
        CHECK_FALSE(1 == 1);
    }
}

TEST_CASE("Timer.Counter", "[timer][counter]")
{
    Counter c;
    {
        Timer<milliseconds> t(c);
        sleep_for(2ms);
    }
    REQUIRE(c.value() > 1);
}

TEST_CASE("Timer.Gauge", "[timer][gauge]")
{
    Gauge g;
    {
        Timer<milliseconds> t(g);
        sleep_for(2ms);
    }
    REQUIRE(g.value() > 1);
}

TEST_CASE("Timer.Histogram", "[timer][histogram]")
{
    Histogram h({ 0.1, 100. });
    {
        Timer<milliseconds> t(h);
        sleep_for(2ms);
    }
    REQUIRE(h.sum() > 1);
    REQUIRE(h.count() == 1);
	auto values = h.values();
	REQUIRE(values[0].second == 0);
	REQUIRE(values[1].second == 1);
}
