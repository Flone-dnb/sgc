// Custom.
#include "GarbageCollector.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("sample test") {
    // sample
    sgc::GarbageCollector::get().collectGarbage();
    REQUIRE(true);
}
