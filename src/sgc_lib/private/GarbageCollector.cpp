#include "GarbageCollector.h"

// Custom.
#include "GcAllocation.h"
#include "GcTypeInfo.h"

namespace sgc {

    GarbageCollector& GarbageCollector::get() {
        static GarbageCollector garbageCollector;
        return garbageCollector;
    }

    void GarbageCollector::collectGarbage() {
        {
            std::scoped_lock guard(mtxPendingNodeGraphChanges.first, mtxRootNodes.first);

            // Apply changes from pending set.
        }

        // Lock root nodes and allocations to make sure new allocations won't be created while we are
        // collecting garbage.
        std::scoped_lock guard(mtxRootNodes.first, mtxAllocationControlledData.first);

        // run garbage collection
    }

    bool GarbageCollector::onGcPointerConstructed(GcPtrBase* pConstructedPtr) {
        {
            std::scoped_lock guard(mtxCurrentlyConstructingObjects.first);

            if (!mtxCurrentlyConstructingObjects.second.empty()) {
                // Iterate over all currently creating allocations from last to the first
                // in case the object of the user-specified type calls `makeGc` in constructor and so on.
                for (auto allocationIt = mtxCurrentlyConstructingObjects.second.rbegin();
                     allocationIt != mtxCurrentlyConstructingObjects.second.rend();
                     ++allocationIt) {
                    // Get allocation and its type.
                    const auto pAllocation = (*allocationIt);
                    const auto pTypeInfo = pAllocation->getTypeInfo();

                    // Check if we already initialized all GC pointer field offsets.
                    if (pTypeInfo->bAllGcPtrFieldOffsetsInitialized) {
                        // Check the next allocation.
                        continue;
                    }

                    // Try registering the offset.
                    if (pTypeInfo->tryRegisteringGcPtrFieldOffset(pConstructedPtr, pAllocation)) {
                        // Found parent object. Exit function.
                        return false;
                    }
                }
            }
        }

        // This pointer is not a field of some object.

        {
            std::scoped_lock guard(mtxPendingNodeGraphChanges.first);

            // Add this pointer as a new root node.
            mtxPendingNodeGraphChanges.second.newRootNodes.insert(pConstructedPtr);
        }

        // This is a root node.
        return true;
    }

}
