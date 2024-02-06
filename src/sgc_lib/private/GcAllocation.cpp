#include "GcAllocation.h"

namespace sgc {

    GcAllocation::GcAllocation(void* pAllocatedMemory, GcTypeInfo* pTypeInfo)
        : pAllocatedMemory(pAllocatedMemory), pTypeInfo(pTypeInfo) {
        // Get allocations info.
        auto& mtxAllocationsInfo = GarbageCollector::get().mtxAllocationControlledData;
        std::scoped_lock guard(mtxAllocationsInfo.first);

        // Add self and allocation info.
        mtxAllocationsInfo.second.existingAllocations.insert(this);
        mtxAllocationsInfo.second.allocationInfoRefs[getAllocationInfo()] = this;
#if defined(DEBUG)
        static_assert(
            sizeof(GarbageCollector::AllocationControlledData) == 160, // NOLINT
            "consider inserting new data");
#endif
    }

    GcAllocation::~GcAllocation() {
        // Get allocated data.
        const auto pAllocationInfo = getAllocationInfo();
        const auto pAllocatedObject = getAllocatedObject();

        // Get allocations info.
        auto& mtxAllocationsInfo = GarbageCollector::get().mtxAllocationControlledData;
        std::scoped_lock guard(mtxAllocationsInfo.first);

        // Remove self and allocation info.
        if (mtxAllocationsInfo.second.existingAllocations.erase(this) != 1) [[unlikely]] {
            GcInfoCallbacks::getWarningCallback()(
                "GC allocation failed to find self (to be erased) in the array of existing allocations");
        }
        if (mtxAllocationsInfo.second.allocationInfoRefs.erase(pAllocationInfo) != 1) [[unlikely]] {
            GcInfoCallbacks::getWarningCallback()("GC allocation failed to its allocation info (to be "
                                                  "erased) in the array of existing allocation infos");
        }
#if defined(DEBUG)
        static_assert(
            sizeof(GarbageCollector::AllocationControlledData) == 160, "consider erasing new data"); // NOLINT
#endif

        // Call destructor.
        pTypeInfo->getInvokeDestructor()(pAllocatedObject);

        // Free the allocated memory.
        ::operator delete(pAllocatedMemory);
    }

    GcTypeInfo* GcAllocation::getTypeInfo() const { return pTypeInfo; }

    void* GcAllocation::getAllocatedObject() const {
        return reinterpret_cast<char*>(pAllocatedMemory) + sizeof(GcAllocationInfo);
    }
}
