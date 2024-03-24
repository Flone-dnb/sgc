// Standard.
#include <utility>
#include <array>
#include <variant>

// Custom.
#include "GarbageCollector.h"
#include "GcPtr.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("storing gc pointer in std::pair does not cause leaks") {
    class Foo {
    public:
        std::pair<std::recursive_mutex, sgc::GcPtr<Foo>> pair;
    };

    {
        auto pFoo = sgc::makeGc<Foo>();

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }

        // Set valid value.
        pFoo->pair.second = pFoo; // create a cyclic reference

        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // only `pFoo` is root node, while the one in the pair is a child node
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("storing gc pointer in std::array does not cause leaks") {
    class Foo {
    public:
        std::array<sgc::GcPtr<Foo>, 1> vArray;
    };

    {
        auto pFoo = sgc::makeGc<Foo>();

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }

        // Set valid value.
        pFoo->vArray[0] = pFoo; // create a cyclic reference

        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // only `pFoo` is root node, while the one in the array is a child node
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
