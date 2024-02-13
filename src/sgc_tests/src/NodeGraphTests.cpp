// Custom.
#include "GarbageCollector.h"
#include "GcPtr.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("use makeGc to create a GcPtr root node") {
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
            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.size() == 1);
            REQUIRE((*pMtxPendingChanges->second.newGcPtrRootNodes.begin())->getUserObject() == pFoo.get());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // There should be no root nodes yet because pending changes were not applied yet.
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }

        // Run GC to apply the changes.
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);

        // Create inner.
        pFoo->pInner = sgc::makeGc<Foo>();

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            // Temporary root node that was created in `makeGc` was created and destroyed.
            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // There should only be 1 root node.
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
            REQUIRE((*pMtxRootNodes->second.gcPtrRootNodes.begin())->getUserObject() == pFoo.get());
        }

        // Clear inner.
        pFoo->pInner = nullptr;

        // Run GC (inner should be deleted).
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            // There should be no nodes.
            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // There should only be our node (because GcPtr object still exists).
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
            REQUIRE((*pMtxRootNodes->second.gcPtrRootNodes.begin())->getUserObject() == pFoo.get());
        }
    }

    // Scope ended and pFoo and pInner were destroyed.

    // Check pending changes.
    const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        // There should only be our destroyed root node.
        REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.size() == 1);
        REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());

        REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
    }

    // Check root nodes.
    const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(pMtxRootNodes->first);

        // There should still be our node.
        REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
        REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
    }

    // Run GC to apply pending changes (Foo should be deleted).
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);

    // Check pending changes.
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        // All empty.
        REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());

        REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
    }

    // Check root nodes.
    {
        std::scoped_lock guard(pMtxRootNodes->first);

        // All empty.
        REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
        REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
    }
}
