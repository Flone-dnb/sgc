// Custom.
#include "GarbageCollector.h"
#include "GcPtr.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("use makeGc to create a root node") {
    // Prepare custom type.
    class Foo {
    public:
        sgc::GcPtr<Foo> pInner;
    };

    {
        // Create root node.
        auto pFoo = sgc::makeGc<Foo>();

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            // There should only be 1 new root node.
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

        // Run GC to apply the changes.
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);

        // Create inner.
        pFoo->pInner = sgc::makeGc<Foo>();

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            // Temporary root node that was created in `makeGc` was created and destroyed.
            REQUIRE(pMtxPendingChanges->second.newRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // There should only be 1 root node.
            REQUIRE(pMtxRootNodes->second.size() == 1);
            REQUIRE((*pMtxRootNodes->second.begin())->getUserObject() == pFoo.get());
        }

        // Clear inner.
        pFoo->pInner = nullptr;

        // Run GC (inner should be deleted).
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);

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

    // Scope ended and pFoo and pInner were destroyed.

    // Check pending changes.
    const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        // There should only be our destroyed root node.
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

    // Run GC to apply pending changes (Foo should be deleted).
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);

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
