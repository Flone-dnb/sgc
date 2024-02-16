// Standard.
#include <utility>

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

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.size() == 1);
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
        }

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }

        // Set valid value.
        pFoo->pair.second = pFoo; // create a cyclic reference

        // Apply changes.
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
        }

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
