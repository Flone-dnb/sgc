#include "GarbageCollector.h"

// Custom.
#include "GcAllocation.h"
#include "GcTypeInfo.h"
#include "GcPtr.h"
#include "GcContainerBase.h"

namespace sgc {

    GarbageCollector& GarbageCollector::get() {
        static GarbageCollector garbageCollector;
        return garbageCollector;
    }

    GarbageCollector::GarbageCollector() {
        // Reserve some space for allocations to be processed.
        vGrayAllocations.reserve(1024); // NOLINT: seems like a good starting capacity
    }

    void GarbageCollector::applyPendingChanges() {
        // Apply changes from pending set.
        std::scoped_lock guard(mtxPendingNodeGraphChanges.first, mtxRootNodes.first);
        auto& changes = mtxPendingNodeGraphChanges.second;

        // Add new GcPtr root nodes.
        for (const auto pNewGcPtrRootNode : changes.newGcPtrRootNodes) {
            mtxRootNodes.second.gcPtrRootNodes.insert(pNewGcPtrRootNode);
        }
        changes.newGcPtrRootNodes.clear();

        // Remove destroyed GcPtr root nodes.
        for (const auto pDestoryedGcPtrRootNode : changes.destroyedGcPtrRootNodes) {
            mtxRootNodes.second.gcPtrRootNodes.erase(pDestoryedGcPtrRootNode);
        }
        changes.destroyedGcPtrRootNodes.clear();

        // Add new GcContainer root nodes.
        for (const auto pNewContainerRootNode : changes.newGcContainerRootNodes) {
            mtxRootNodes.second.gcContainerRootNodes.insert(pNewContainerRootNode);
        }
        changes.newGcContainerRootNodes.clear();

        // Remove destroyed GcContainer root nodes.
        for (const auto pDestroyedContainerRootNode : changes.destroyedGcContainerRootNodes) {
            mtxRootNodes.second.gcContainerRootNodes.erase(pDestroyedContainerRootNode);
        }
        changes.destroyedGcContainerRootNodes.clear();

#if defined(DEBUG)
        static_assert(sizeof(PendingNodeGraphChanges) == 320, "consider applying new changes"); // NOLINT
#endif
    }

    size_t GarbageCollector::collectGarbage() {
        applyPendingChanges();

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

        // Prepare lambda to "mark" container items.
        const auto markContainerItems = [this](const GcContainerBase* pContainer) {
            // We know that GC containers don't point to allocations,
            // thus just iterate over GcPtr items of this container.
            pContainer->getFunctionToIterateOverGcPtrItems()(pContainer, [this](const GcPtrBase* pGcPtrItem) {
                // Make sure this pointer references an allocation.
                if (pGcPtrItem->pAllocation == nullptr) {
                    return;
                }

                if (pGcPtrItem->pAllocation->getAllocationInfo()->color != GcAllocationColor::WHITE) {
                    // We already found pointer(s) to this allocation so skip processing it.
                    return;
                }

                // Add the allocation to be processed later.
                vGrayAllocations.push_back(pGcPtrItem->pAllocation);
            });
        };

        // Prepare a lambda to do the "mark" step.
        const auto markAllocationAndProcessFields = [this, &markContainerItems](GcAllocation* pAllocation) {
            // Mark this object in black.
            pAllocation->getAllocationInfo()->color = GcAllocationColor::BLACK;

#if defined(DEBUG)
            // Make sure GcPtr field offsets are initialized.
            if (!pAllocation->getTypeInfo()->bAllGcNodeFieldOffsetsInitialized) [[unlikely]] {
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

            // Now iterate over GcContainer fields of the allocation.
            for (const auto& iGcContainerFieldOffset : pAllocation->getTypeInfo()->vGcContainerFieldOffsets) {
                // Get address of GcContainer field.
                const auto pGcContainerField = reinterpret_cast<GcContainerBase*>(
                    reinterpret_cast<char*>(pAllocation->getAllocatedObject()) +
                    static_cast<uintptr_t>(iGcContainerFieldOffset));

                // Mark container items.
                markContainerItems(pGcContainerField);
            }
        };

        // Start marking phase from root GcPtr nodes.
        auto& rootSet = mtxRootNodes.second;
        for (auto ptrIt = rootSet.gcPtrRootNodes.begin(); ptrIt != rootSet.gcPtrRootNodes.end(); ++ptrIt) {
            // Make sure this GcPtr points to a valid allocation.
            const auto pGcPtr = *ptrIt;
            if (pGcPtr->pAllocation == nullptr) {
                // This may happen and it's perfectly fine.
                continue;
            }

            // Process root node.
            markAllocationAndProcessFields(pGcPtr->pAllocation);

            // Process pending allocations.
            while (!vGrayAllocations.empty()) {
                // Get allocation from gray array.
                const auto pAllocation = vGrayAllocations.back(); // copy
                vGrayAllocations.pop_back();

                // Process "gray" allocation.
                markAllocationAndProcessFields(pAllocation);
            }
        }

        // Now iterate over root GcContainer nodes.
        for (auto ptrIt = rootSet.gcContainerRootNodes.begin(); ptrIt != rootSet.gcContainerRootNodes.end();
             ++ptrIt) {
            markContainerItems(*ptrIt);

            // Process pending allocations.
            while (!vGrayAllocations.empty()) {
                // Get allocation from gray array.
                const auto pAllocation = vGrayAllocations.back(); // copy
                vGrayAllocations.pop_back();

                // Process "gray" allocation.
                markAllocationAndProcessFields(pAllocation);
            }
        }

        // Now do the "sweep" phase.
        size_t iDeletedObjectCount = 0;
        for (auto allocationIt = mtxAllocationData.second.existingAllocations.begin();
             allocationIt != mtxAllocationData.second.existingAllocations.end();) {
            // Check allocation color.
            const auto pAllocation = *allocationIt;
            if (pAllocation->getAllocationInfo()->color != GcAllocationColor::WHITE) {
                // We should keep this allocation, switch to the next one.
                ++allocationIt;
                continue;
            }

            // Remove the allocation (update our iterator).
            allocationIt = mtxAllocationData.second.existingAllocations.erase(allocationIt);

            // Remove the allocation's info.
            if (mtxAllocationData.second.allocationInfoRefs.erase(pAllocation->getAllocationInfo()) != 1)
                [[unlikely]] {
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
            iDeletedObjectCount += 1;
        }

        return iDeletedObjectCount;
    }

    size_t GarbageCollector::getAliveAllocationCount() {
        std::scoped_lock guard(mtxAllocationData.first);
        return mtxAllocationData.second.existingAllocations.size();
    }

    std::pair<std::mutex, GarbageCollector::PendingNodeGraphChanges>*
    GarbageCollector::getPendingNodeGraphChanges() {
        return &mtxPendingNodeGraphChanges;
    }

    std::pair<std::recursive_mutex, GarbageCollector::RootNodes>* GarbageCollector::getRootNodes() {
        return &mtxRootNodes;
    }

    std::recursive_mutex* GarbageCollector::getGarbageCollectionMutex() { return &mtxAllocationData.first; }

    bool GarbageCollector::onGcNodeConstructed(GcNode* pConstructedNode) {
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

                    // Try registering the offset.
                    if (pTypeInfo->tryRegisteringGcNodeFieldOffset(pConstructedNode, pAllocation)) {
                        // Found parent object. Exit function.
                        return false;
                    }
                }
            }
        }

        // This node is not a field of some object.

        {
            std::scoped_lock guard(mtxPendingNodeGraphChanges.first);

            // Add this node as a new root node.
            if (const auto pGcContainerNode = dynamic_cast<GcContainerBase*>(pConstructedNode)) {
                mtxPendingNodeGraphChanges.second.newGcContainerRootNodes.insert(pGcContainerNode);
            } else if (const auto pGcPtrNode = dynamic_cast<GcPtrBase*>(pConstructedNode)) {
                mtxPendingNodeGraphChanges.second.newGcPtrRootNodes.insert(pGcPtrNode);
            } else [[unlikely]] {
                GcInfoCallbacks::getCriticalErrorCallback()("found unexpected constructed node type");
                throw std::runtime_error("critical error");
            }
        }

        // This is a root node.
        return true;
    }

    void GarbageCollector::onGcRootNodeBeingDestroyed(GcNode* pRootNode) {
        std::scoped_lock guard(mtxPendingNodeGraphChanges.first);

        if (const auto pGcContainerNode = dynamic_cast<GcContainerBase*>(pRootNode)) {
            // First check if this root node still exists in the pending changes.
            if (mtxPendingNodeGraphChanges.second.newGcContainerRootNodes.erase(pGcContainerNode) > 0) {
                // Looks like this pointer was created and deleted in between garbage collections
                // so just remove it from new root nodes.
                return;
            }

            // Add to destroyed root nodes.
            mtxPendingNodeGraphChanges.second.destroyedGcContainerRootNodes.insert(pGcContainerNode);
        } else if (const auto pGcPtrNode = dynamic_cast<GcPtrBase*>(pRootNode)) {
            // Same thing as above.
            if (mtxPendingNodeGraphChanges.second.newGcPtrRootNodes.erase(pGcPtrNode) > 0) {
                return;
            }
            mtxPendingNodeGraphChanges.second.destroyedGcPtrRootNodes.insert(pGcPtrNode);
        } else [[unlikely]] {
            GcInfoCallbacks::getCriticalErrorCallback()("found unexpected constructed node type");
            throw std::runtime_error("critical error");
        }
    }

    GarbageCollector::PendingNodeGraphChanges::PendingNodeGraphChanges() {
        constexpr size_t iReservedCount = 512; // NOLINT: seems like a good starting capacity

        // Reserve some space.
        newGcPtrRootNodes.reserve(iReservedCount);
        newGcContainerRootNodes.reserve(iReservedCount);
        destroyedGcPtrRootNodes.reserve(iReservedCount);
        destroyedGcContainerRootNodes.reserve(iReservedCount);

#if defined(DEBUG)
        static_assert(
            sizeof(PendingNodeGraphChanges) == 320, "consider adding reserve to new fields"); // NOLINT
#endif
    }
}
