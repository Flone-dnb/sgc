#include "GcAllocationConstructionGuard.h"

// Custom.
#include "GarbageCollector.h"
#include "GcInfoCallbacks.hpp"

namespace sgc {
    GcAllocationConstructionGuard::GcAllocationConstructionGuard(GcAllocation* pAllocation)
        : pAllocation(pAllocation) {
        // Get array of creating objects.
        auto& mtxCreatingObjects = GarbageCollector::get().mtxCurrentlyConstructingObjects;

        std::scoped_lock guard(mtxCreatingObjects.first);

        // Add allocation.
        mtxCreatingObjects.second.push_back(pAllocation);
    }

    GcAllocationConstructionGuard::~GcAllocationConstructionGuard() {
        // Get array of creating objects.
        auto& mtxCreatingObjects = GarbageCollector::get().mtxCurrentlyConstructingObjects;

        std::scoped_lock guard(mtxCreatingObjects.first);

        // Find this allocation.
        for (auto allocationIt = mtxCreatingObjects.second.begin();
             allocationIt != mtxCreatingObjects.second.end();
             ++allocationIt) {
            // Check if this is our allocation.
            if (pAllocation != (*allocationIt)) {
                continue;
            }

            // Remove allocation.
            mtxCreatingObjects.second.erase(allocationIt);
            return;
        }

        // Although this will probably never happen, still add a check for it.
        GcInfoCallbacks::getCriticalErrorCallback()(
            "failed to find previously added allocation in the array of currently constructing objects");
        // don't throw in destructor
    }
}
