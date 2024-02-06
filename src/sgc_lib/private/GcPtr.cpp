#include "GcPtr.h"

// Custom.
#include "GarbageCollector.h"
#include "GcInfoCallbacks.hpp"

namespace sgc {

    GcPtrBase::GcPtrBase() {
        // Notify garbage collector.
        bIsRootNode = GarbageCollector::get().onGcPointerConstructed(this);
    }

    void GcPtrBase::setAllocationFromUserObject(void* pUserObject) {
        // Prepare the error message in case we need it.
        static constexpr auto pNotGcPointerErrorMessage =
            "failed to set the specified raw pointer to a GC pointer because the specified object "
            "(in the raw pointer) was not previously created from `makeGc` call";

        // Make sure we are not running a garbage collection.
        auto& mtxAllocationsData = GarbageCollector::get().mtxAllocationControlledData;
        std::scoped_lock guard(mtxAllocationsData.first);

        // Check if the specified pointer is valid.
        if (pUserObject == nullptr) {
            // Just clear the pointer (keep the GC mutex locked while changing the pointer).
            pAllocation = nullptr;
            return;
        }

        // Prepare addresses.
        const auto iTargetObjectAddress = reinterpret_cast<uintptr_t>(pUserObject);
        constexpr auto iAllocationInfoSize = sizeof(GcAllocationInfo);

        // Make sure there is a space for the allocation info object.
        if (iTargetObjectAddress < iAllocationInfoSize) [[unlikely]] {
            // Not a valid GC object.
            GcInfoCallbacks::getCriticalErrorCallback()(pNotGcPointerErrorMessage);
            throw std::runtime_error(pNotGcPointerErrorMessage);
        }

        // Calculate the address of the possible allocation info object.
        const auto pNewAllocationInfo = reinterpret_cast<GcAllocationInfo*>(
            reinterpret_cast<char*>(pUserObject) - static_cast<uintptr_t>(iAllocationInfoSize));

        // Find this allocation in the garbage collector's "database" to make sure the pointer is valid.
        auto& allocationInfos = mtxAllocationsData.second.allocationInfoRefs;
        const auto allocationInfoIt = allocationInfos.find(pNewAllocationInfo);
        if (allocationInfoIt == allocationInfos.end()) [[unlikely]] {
            // Not a valid GC object.
            GcInfoCallbacks::getCriticalErrorCallback()(pNotGcPointerErrorMessage);
            throw std::runtime_error(pNotGcPointerErrorMessage);
        }

        // Save allocation.
        pAllocation = allocationInfoIt->second;
    }

    GcPtrBase::~GcPtrBase() {
        if (bIsRootNode) {
            auto& mtxPendingChanges = GarbageCollector::get().mtxPendingNodeGraphChanges;
            std::scoped_lock guard(mtxPendingChanges.first);

            mtxPendingChanges.second.destroyedRootNodes.insert(this);
        }
    }

    GcAllocationInfo* GcPtrBase::getAllocationInfo() const {
        // Make sure allocation is valid.
        if (pAllocation == nullptr) {
            return nullptr;
        }

        return pAllocation->getAllocationInfo();
    }

    void* GcPtrBase::getUserObject() const {
        // Make sure allocation is valid.
        if (pAllocation == nullptr) {
            return nullptr;
        }

        return pAllocation->getAllocatedObject();
    }

}
