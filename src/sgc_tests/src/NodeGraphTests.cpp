// Custom.
#include "GarbageCollector.h"
#include "GcPtr.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("use makeGc to create a root node") {
    // Prepare custom type.
    class Foo {};

    {
        // Create root node.
        auto pFoo = sgc::makeGc<Foo>();

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            // There should only be our node.
            REQUIRE(pMtxPendingChanges->second.newRootNodes.size() == 1);
            REQUIRE((*pMtxPendingChanges->second.newRootNodes.begin())->getUserObject() == pFoo.get());
            REQUIRE(pMtxPendingChanges->second.destroyedRootNodes.empty());
        }

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // There should be no root nodes yet because pending changes were not applied yet.
            REQUIRE(pMtxRootNodes->second.empty());
        }

        // Run GC.
        sgc::GarbageCollector::get().collectGarbage();

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            // There should be no nodes.
            REQUIRE(pMtxPendingChanges->second.newRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // There should only be our node.
            REQUIRE(pMtxRootNodes->second.size() == 1);
            REQUIRE((*pMtxRootNodes->second.begin())->getUserObject() == pFoo.get());
        }

        // Clear pointer.
        pFoo = nullptr;

        // Run GC.
        sgc::GarbageCollector::get().collectGarbage();

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            // There should be no nodes.
            REQUIRE(pMtxPendingChanges->second.newRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // There should only be our node (because GcPtr object still exists).
            REQUIRE(pMtxRootNodes->second.size() == 1);
            REQUIRE((*pMtxRootNodes->second.begin())->getUserObject() == pFoo.get());
        }
    }

    // Scope ended and pFoo was destroyed.

    // Check pending changes.
    const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        // There should only be our destroyed node.
        REQUIRE(pMtxPendingChanges->second.destroyedRootNodes.size() == 1);
        REQUIRE(pMtxPendingChanges->second.newRootNodes.empty());
    }

    // Check root nodes.
    const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(pMtxRootNodes->first);

        // There should still be our node.
        REQUIRE(pMtxRootNodes->second.size() == 1);
    }

    // Run GC to apply pending changes.
    sgc::GarbageCollector::get().collectGarbage();

    // Check pending changes.
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        // All empty.
        REQUIRE(pMtxPendingChanges->second.destroyedRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.newRootNodes.empty());
    }

    // Check root nodes.
    {
        std::scoped_lock guard(pMtxRootNodes->first);

        // All empty.
        REQUIRE(pMtxRootNodes->second.empty());
    }
}
