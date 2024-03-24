#include "GarbageCollector.h"

// Custom.
#include "GcAllocation.h"
#include "GcTypeInfo.h"
#include "GcPtr.h"
#include "GcContainerBase.h"
#include "DebugLogger.hpp"

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

        SGC_DEBUG_LOG("applying pending changes");

        // It's important to first remove destroyed nodes and only then add created
        // because we had a bug where GcPtr was being created and destroyed in the loop
        // then GcPtr from the previous iteration was added as destroyed and
        // on the new iteration a new GcPtr was using the same address as from the previous
        // iteration (and added itself as a new root node) thus we were failing because
        // we were first adding GcPtr from the new iteration and then removing GcPtr
        // from the previous iteration but both had the same address so we were
        // accidentally removing a valid node in the node graph

        // Remove destroyed GcPtr root nodes.
        for (const auto pDestoryedGcPtrRootNode : changes.destroyedGcPtrRootNodes) {
            if (mtxRootNodes.second.gcPtrRootNodes.erase(pDestoryedGcPtrRootNode) != 1) [[unlikely]] {
                // Something is wrong.
                GcInfoCallbacks::getCriticalErrorCallback()(
                    "a destroyed GcPtr root node is marked as pending to be removed from GC but it's not "
                    "found in the root set");
                throw std::runtime_error("critical error");
            }
            SGC_DEBUG_LOG(std::format(
                "removed destroyed root GcPtr {}", reinterpret_cast<uintptr_t>(pDestoryedGcPtrRootNode)));
        }
        changes.destroyedGcPtrRootNodes.clear();

        // Remove destroyed GcContainer root nodes.
        for (const auto pDestroyedContainerRootNode : changes.destroyedGcContainerRootNodes) {
            if (mtxRootNodes.second.gcContainerRootNodes.erase(pDestroyedContainerRootNode) != 1)
                [[unlikely]] {
                // Something is wrong.
                GcInfoCallbacks::getCriticalErrorCallback()("a destroyed GcContainer root node is marked as "
                                                            "pending to be removed from GC but it's not "
                                                            "found in the root set");
                throw std::runtime_error("critical error");
            }
            SGC_DEBUG_LOG(std::format(
                "removed destroyed root GcContainer {}",
                reinterpret_cast<uintptr_t>(pDestroyedContainerRootNode)));
        }
        changes.destroyedGcContainerRootNodes.clear();

        //
        // Only after removing all destroyed nodes, add new ones.
        //

        // Add new GcPtr root nodes.
        for (const auto pNewGcPtrRootNode : changes.newGcPtrRootNodes) {
            mtxRootNodes.second.gcPtrRootNodes.insert(pNewGcPtrRootNode);
            SGC_DEBUG_LOG(
                std::format("added new root GcPtr {}", reinterpret_cast<uintptr_t>(pNewGcPtrRootNode)));
        }
        changes.newGcPtrRootNodes.clear();

        // Add new GcContainer root nodes.
        for (const auto pNewContainerRootNode : changes.newGcContainerRootNodes) {
            mtxRootNodes.second.gcContainerRootNodes.insert(pNewContainerRootNode);
            SGC_DEBUG_LOG(std::format(
                "added new root GcContainer {}", reinterpret_cast<uintptr_t>(pNewContainerRootNode)));
        }
        changes.newGcContainerRootNodes.clear();

#if defined(DEBUG)
        static_assert(sizeof(PendingNodeGraphChanges) == 320, "consider applying new changes"); // NOLINT
#endif
    }

    size_t GarbageCollector::collectGarbage() {
        // - Lock root nodes and allocations to make sure new allocations won't be created while we are
        // collecting garbage.
        // - Also lock pending nodes to make sure no root GcPtr will be destroyed (in the other thread) while
        // we are here (since we will iterate over them).
        // - GcPtr locks allocations when changing its pointer so we guarantee that no GcPtr will change
        // its pointer while we are in the GC (same thing with GcContainer).
        // - Also no GC node will be destroyed while GC is running (since GcPtr and GcContainer
        // lock GC mutex in destructor).
        std::scoped_lock guard(mtxRootNodes.first, mtxAllocationData.first, mtxPendingNodeGraphChanges.first);

        SGC_DEBUG_LOG("GC started");

        // After locking mutexes apply pending changes.
        applyPendingChanges();

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
            SGC_DEBUG_LOG(std::format(
                "processing root GcPtr {} with allocation {}",
                reinterpret_cast<uintptr_t>(pGcPtr),
                reinterpret_cast<uintptr_t>(pGcPtr->pAllocation)));
            markAllocationAndProcessFields(pGcPtr->pAllocation);

            // Process pending allocations.
            while (!vGrayAllocations.empty()) {
                // Get allocation from gray array.
                const auto pAllocation = vGrayAllocations.back(); // copy
                vGrayAllocations.pop_back();

                SGC_DEBUG_LOG(std::format(
                    "processing allocation with user object {} from gray set",
                    reinterpret_cast<uintptr_t>(pAllocation->getAllocatedObject())));

                // Process "gray" allocation.
                markAllocationAndProcessFields(pAllocation);
            }
        }

        SGC_DEBUG_LOG("starting to process root GcContainers");

        // Now iterate over root GcContainer nodes.
        for (auto ptrIt = rootSet.gcContainerRootNodes.begin(); ptrIt != rootSet.gcContainerRootNodes.end();
             ++ptrIt) {
            markContainerItems(*ptrIt);

            // Process pending allocations.
            while (!vGrayAllocations.empty()) {
                // Get allocation from gray array.
                const auto pAllocation = vGrayAllocations.back(); // copy
                vGrayAllocations.pop_back();

                SGC_DEBUG_LOG(std::format(
                    "processing allocation with user object {} from gray set",
                    reinterpret_cast<uintptr_t>(pAllocation->getAllocatedObject())));

                // Process "gray" allocation.
                markAllocationAndProcessFields(pAllocation);
            }
        }

        SGC_DEBUG_LOG("GC sweep started");

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
                GcInfoCallbacks::getWarningCallback()(
                    "GC allocation failed to find its allocation info (to be "
                    "erased) in the array of existing allocation info objects");
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

        SGC_DEBUG_LOG("GC ended");

        return iDeletedObjectCount;
    }

    size_t GarbageCollector::getAliveAllocationCount() {
        std::scoped_lock guard(mtxAllocationData.first);
        return mtxAllocationData.second.existingAllocations.size();
    }

    std::pair<std::recursive_mutex, GarbageCollector::PendingNodeGraphChanges>*
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

            SGC_DEBUG_LOG(std::format(
                "GcPtr {} was added as a pending root node", reinterpret_cast<uintptr_t>(pConstructedNode)));
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
                // so removing it from the pending new root nodes is enough.
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
