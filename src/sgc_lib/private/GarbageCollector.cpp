#include "GarbageCollector.h"

// Standard.
#include <stdexcept>

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

    size_t GarbageCollector::collectGarbage() {
        // - Lock mutex to make sure new allocations won't be created while we are collecting garbage.
        // - GcPtr locks mutex when changing its pointer so we guarantee that no GcPtr will change
        // its pointer while we are in the GC (same thing with GcContainer).
        // - No GC node will be created/destroyed while GC is running (since GcPtr and GcContainer
        // lock GC mutex in constructor/destructor).
        std::scoped_lock guardAllocations(mtxGcData.first);

        SGC_DEBUG_LOG("GC started");

        // Before running the "mark" step color every allocation in white
        // (to color only referenced allocations in black in the "mark" step).
        auto& existingAllocations = mtxGcData.second.allocationData.existingAllocations;
        for (auto allocationIt = existingAllocations.begin(); allocationIt != existingAllocations.end();
             ++allocationIt) {
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
        auto& rootSet = mtxGcData.second.rootNodes;
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
        for (auto allocationIt = existingAllocations.begin(); allocationIt != existingAllocations.end();) {
            // Check allocation color.
            const auto pAllocation = *allocationIt;
            if (pAllocation->getAllocationInfo()->color != GcAllocationColor::WHITE) {
                // We should keep this allocation, switch to the next one.
                ++allocationIt;
                continue;
            }

            // Remove the allocation (update our iterator).
            allocationIt = existingAllocations.erase(allocationIt);

            // Remove the allocation's info.
            if (mtxGcData.second.allocationData.allocationInfoRefs.erase(pAllocation->getAllocationInfo()) !=
                1) [[unlikely]] {
                GcInfoCallbacks::getWarningCallback()(
                    "GC allocation failed to find its allocation info (to be "
                    "erased) in the array of existing allocation info objects");
            }

            // Delete (free) the allocation.
            delete pAllocation;
            iDeletedObjectCount += 1;
        }

        SGC_DEBUG_LOG("GC ended");

        return iDeletedObjectCount;
    }

    size_t GarbageCollector::getAliveAllocationCount() {
        std::scoped_lock guard(mtxGcData.first);
        return mtxGcData.second.allocationData.existingAllocations.size();
    }

    std::pair<std::recursive_mutex*, GarbageCollector::RootNodes*> GarbageCollector::getRootNodes() {
        return std::make_pair(&mtxGcData.first, &mtxGcData.second.rootNodes);
    }

    std::recursive_mutex* GarbageCollector::getGarbageCollectionMutex() { return &mtxGcData.first; }

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
            std::scoped_lock guard(mtxGcData.first);

            auto& rootSet = mtxGcData.second.rootNodes;

            // Add this node as a new root node.
            if (const auto pGcContainerNode = dynamic_cast<GcContainerBase*>(pConstructedNode)) {
                rootSet.gcContainerRootNodes.insert(pGcContainerNode);
            } else if (const auto pGcPtrNode = dynamic_cast<GcPtrBase*>(pConstructedNode)) {
                rootSet.gcPtrRootNodes.insert(pGcPtrNode);
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
        std::scoped_lock guard(mtxGcData.first);

        auto& rootSet = mtxGcData.second.rootNodes;

        if (const auto pGcContainerNode = dynamic_cast<GcContainerBase*>(pRootNode)) {
            if (rootSet.gcContainerRootNodes.erase(pGcContainerNode) != 1) [[unlikely]] {
                // Something is wrong.
                GcInfoCallbacks::getCriticalErrorCallback()(
                    "GC container root node is being destroyed but it's not found in the root set");
                throw std::runtime_error("critical error");
            }
        } else if (const auto pGcPtrNode = dynamic_cast<GcPtrBase*>(pRootNode)) {
            if (rootSet.gcPtrRootNodes.erase(pGcPtrNode) != 1) [[unlikely]] {
                // Something is wrong.
                GcInfoCallbacks::getCriticalErrorCallback()(
                    "GC pointer root node is being destroyed but it's not found in the root set");
                throw std::runtime_error("critical error");
            }
        } else [[unlikely]] {
            GcInfoCallbacks::getCriticalErrorCallback()("found unexpected constructed node type");
            throw std::runtime_error("critical error");
        }
    }
}
