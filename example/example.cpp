#include <iostream>
#include <string>
#include <unistd.h>
#include <chrono>

#include <metrics/registry.h>
#include <metrics/serialize.h>
#include <metrics/timer.h>


void timerExample() {
    Metrics::Histogram histogram = Metrics::defaultRegistry().getHistogram({"myTimer"}, {1., 2., 5., 10.});
    Metrics::Timer<std::chrono::seconds> timer(histogram);
    int randNum = (rand() % 3) + 1;
    sleep(randNum);
}

void metricsTest() {
    Metrics::defaultRegistry().getCounter({"birds", {{"kind", "sparrow" }}})++;
    Metrics::defaultRegistry().getCounter({"birds", {{"kind", "robin" }}})++;
    Metrics::defaultRegistry().getCounter({"helloworld"})++;
    Metrics::defaultRegistry().getCounter({"helloworld"})++;

    Metrics::defaultRegistry().getText({"bar", {{"kind", "msg" }}}) = "Hello World!";
    Metrics::defaultRegistry().getText({"bar", {{"kind", "error" }}}) = "";

    Metrics::Gauge g = Metrics::defaultRegistry().getGauge({"foo"});
    g = 378924;
    g = 111256;
    g += 3;
    
    timerExample();
    timerExample();

    // Save metrics to file
    Metrics::serializeJSON(Metrics::defaultRegistry(), "example.json");
    
    // Get metrics JSON string
    std::string strMetrics = Metrics::serializeJSON(Metrics::defaultRegistry(), "");
    std::cout << "Metrics[1]: " << strMetrics.c_str() << std::endl;

    // Clean metrics
    Metrics::defaultRegistry().clean();
    strMetrics = Metrics::serializeJSON(Metrics::defaultRegistry(), "");
    std::cout << "Metrics[2]: " << strMetrics.c_str() << std::endl;

    timerExample();
    // Get metrics JSON string
    strMetrics = Metrics::serializeJSON(Metrics::defaultRegistry(), "");
    std::cout << "Metrics[3]: " << strMetrics.c_str() << std::endl;
}

int main() {
    srand(time(NULL));
    std::cout << "Hello World!" << std::endl;
    metricsTest();
    std::cout << "Goodbye World!" << std::endl;
    return 0;
}