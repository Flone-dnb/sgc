// Custom.
#include "GarbageCollector.h"
#include "GcPtr.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("use makeGc to create root node") {
    // Prepare custom type.
    class Foo {};

    // Create root node.
    auto pFoo = sgc::makeGc<Foo>();

    // Get pending changes.
    const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        // There should only be our node.
        REQUIRE(pMtxPendingChanges->second.newRootNodes.size() == 1);
        REQUIRE((*pMtxPendingChanges->second.newRootNodes.begin())->getUserObject() == pFoo.get());
    }

    // Get root nodes.
    const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(pMtxRootNodes->first);

        // There should be no root nodes yet because pending changes were not applied yet.
        REQUIRE(pMtxRootNodes->second.empty());
    }
}
