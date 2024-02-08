#include "GarbageCollector.h"

// Custom.
#include "GcAllocation.h"
#include "GcTypeInfo.h"
#include "GcPtr.h"

namespace sgc {

    GarbageCollector& GarbageCollector::get() {
        static GarbageCollector garbageCollector;
        return garbageCollector;
    }

    GarbageCollector::GarbageCollector() {
        // Reserve some space for allocations to be processed.
        vGrayAllocations.reserve(512); // NOLINT: seems like a good starting capacity
    }

    void GarbageCollector::collectGarbage() {
        {
            // Apply changes from pending set.
            std::scoped_lock guard(mtxPendingNodeGraphChanges.first, mtxRootNodes.first);
            auto& changes = mtxPendingNodeGraphChanges.second;

            // Add new root nodes first.
            for (auto ptrIt = changes.newRootNodes.begin(); ptrIt != changes.newRootNodes.end(); ++ptrIt) {
                mtxRootNodes.second.insert(*ptrIt);
            }
            changes.newRootNodes.clear();

            // Only then remove deleted root nodes
            // because if there was a GcPtr root node that was created and deleted inverse order
            // would cause us to add deleted root node.
            for (auto ptrIt = changes.destroyedRootNodes.begin(); ptrIt != changes.destroyedRootNodes.end();
                 ++ptrIt) {
                mtxRootNodes.second.erase(*ptrIt);
            }
            changes.destroyedRootNodes.clear();

#if defined(DEBUG)
            static_assert(sizeof(PendingNodeGraphChanges) == 160, "consider applying new changes"); // NOLINT
#endif
        }

        // Lock root nodes and allocations to make sure new allocations won't be created
        // while we are collecting garbage.
        // Also GcPtr locks allocations when changing its pointer so we guarantee
        // that no GcPtr will change its pointer while we are in the GC.
        std::scoped_lock guard(mtxRootNodes.first, mtxAllocationData.first);

        // Before running the "mark" step color every allocation in white
        // (to color only referenced allocations in black in the "mark" step).
        auto& allocations = mtxAllocationData.second.existingAllocations;
        for (auto allocationIt = allocations.begin(); allocationIt != allocations.end(); ++allocationIt) {
            (*allocationIt)->getAllocationInfo()->color = GcAllocationColor::WHITE;
        }

        // Prepare a lambda to do the "mark" step.
        const auto markAllocationAndProcessFields = [this](GcAllocation* pAllocation) {
            // Mark this object in black.
            pAllocation->getAllocationInfo()->color = GcAllocationColor::BLACK;

#if defined(DEBUG)
            // Make sure GcPtr field offsets are initialized.
            if (!pAllocation->getTypeInfo()->bAllGcPtrFieldOffsetsInitialized) [[unlikely]] {
                GcInfoCallbacks::getCriticalErrorCallback()(
                    "found type info with uninitialized field offsets");
                throw std::runtime_error("critical error");
            }
#endif

            // Iterate over GcPtr fields of the allocation.
            for (const auto& iGcPtrFieldOffset : pAllocation->getTypeInfo()->vGcPtrFieldOffsets) {
                // Get address of GcPtr field.
                const auto pGcPtrField = reinterpret_cast<GcPtrBase*>(
                    reinterpret_cast<char*>(pAllocation->getAllocatedObject()) +
                    static_cast<uintptr_t>(iGcPtrFieldOffset));

                // Make sure this pointer references an allocation.
                if (pGcPtrField->pAllocation == nullptr) {
                    // Check the next GcPtr field.
                    continue;
                }

                if (pGcPtrField->pAllocation->getAllocationInfo()->color != GcAllocationColor::WHITE) {
                    // We already found pointer(s) to this allocation so skip processing it.
                    continue;
                }

                // Add the allocation to be processed later.
                vGrayAllocations.push_back(pGcPtrField->pAllocation);
            }
        };

        // Start marking phase from root nodes.
        for (auto ptrIt = mtxRootNodes.second.begin(); ptrIt != mtxRootNodes.second.end(); ++ptrIt) {
            // Make sure this GcPtr points to a valid allocation.
            const auto pGcPtr = *ptrIt;
            if (pGcPtr->pAllocation == nullptr) {
                continue;
            }

            // Process root node.
            markAllocationAndProcessFields(pGcPtr->pAllocation);

            while (!vGrayAllocations.empty()) {
                // Get allocation from gray array.
                const auto pAllocation = vGrayAllocations.back(); // copy
                vGrayAllocations.pop_back();

                // Process "gray" allocation.
                markAllocationAndProcessFields(pAllocation);
            }
        }

        // Now do the "sweep" phase.
        for (auto allocationIt = mtxAllocationData.second.existingAllocations.begin();
             allocationIt != mtxAllocationData.second.existingAllocations.end();) {
            // Check allocation color.
            const auto pAllocation = *allocationIt;
            if (pAllocation->getAllocationInfo()->color != GcAllocationColor::WHITE) {
                // We should keep this allocation, switch to the next one.
                ++allocationIt;
                continue;
            }

            // Remove the allocation.
            allocationIt = mtxAllocationData.second.existingAllocations.erase(allocationIt);

            // Remove the allocation's info.
            if (mtxAllocationData.second.allocationInfoRefs.erase(
                    pAllocation->getAllocationInfo()) != 1) [[unlikely]] {
                GcInfoCallbacks::getWarningCallback()("GC allocation failed to its allocation info (to be "
                                                      "erased) in the array of existing allocation infos");
            }

#if defined(DEBUG)
            static_assert(
                sizeof(GarbageCollector::AllocationData) == 160, // NOLINT
                "consider erasing new data");
#endif

            // Delete (free) the allocation.
            delete pAllocation;
        }
    }

    std::pair<std::mutex, GarbageCollector::PendingNodeGraphChanges>*
    GarbageCollector::getPendingNodeGraphChanges() {
        return &mtxPendingNodeGraphChanges;
    }

    std::pair<std::recursive_mutex, std::unordered_set<const GcPtrBase*>>* GarbageCollector::getRootNodes() {
        return &mtxRootNodes;
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

    void GarbageCollector::onRootNodeGcPointerDestroyed(GcPtrBase* pDestroyedRootPtr) {
        std::scoped_lock guard(mtxPendingNodeGraphChanges.first);

        // First check if this root node still exists in the pending changes.
        if (mtxPendingNodeGraphChanges.second.newRootNodes.erase(pDestroyedRootPtr) > 0) {
            // Looks like this pointer was created and deleted in between garbage collections
            // so just remove it from new root nodes.
            return;
        }

        mtxPendingNodeGraphChanges.second.destroyedRootNodes.insert(pDestroyedRootPtr);
    }

    GarbageCollector::PendingNodeGraphChanges::PendingNodeGraphChanges() {
        constexpr size_t iReservedCount = 256; // NOLINT: seems like a good starting capacity

        // Reserve some space.
        newRootNodes.reserve(iReservedCount);
        destroyedRootNodes.reserve(iReservedCount);

#if defined(DEBUG)
        static_assert(
            sizeof(PendingNodeGraphChanges) == 160, "consider adding reserve to new fields"); // NOLINT
#endif
    }

}
