// Custom.
#include "GarbageCollector.h"
#include "GcVector.hpp"
#include "GcPtr.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("new vector elements are not registered as root nodes") {
    {
        // Create container.
        sgc::GcVector<sgc::GcPtr<int>> vTest;

        {
            // Create some pointer.
            sgc::GcPtr<int> pValue = sgc::makeGc<int>(1);

            // Insert.
            vTest.push_back(sgc::GcPtr<int>()); // insert empty pointer
            vTest.push_back(pValue);            // insert non-empty pointer

            // Get pending changes.
            const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
            {
                std::scoped_lock guard(pMtxPendingChanges->first);

                REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.size() == 1);
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

            // Run GC to apply the changes.
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
            REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

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

                REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.size() == 1);
                REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
            }
        } // pValue destroyed here

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.size() == 1);
        }

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.size() == 1);
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
        }

        // Object should still be alive (since there is a pointer in our vector).
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

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

            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.size() == 1);
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
        }
    }

    // Can now be deleted.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
