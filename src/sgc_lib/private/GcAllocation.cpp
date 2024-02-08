#include "GcAllocation.h"

namespace sgc {

    GcAllocation::GcAllocation(void* pAllocatedMemory, GcTypeInfo* pTypeInfo)
        : pAllocatedMemory(pAllocatedMemory), pTypeInfo(pTypeInfo) {
        // Get allocations info.
        auto& mtxAllocationsInfo = GarbageCollector::get().mtxAllocationData;
        std::scoped_lock guard(mtxAllocationsInfo.first);

        // Add self and allocation info.
        mtxAllocationsInfo.second.existingAllocations.insert(this);
        mtxAllocationsInfo.second.allocationInfoRefs[getAllocationInfo()] = this;
#if defined(DEBUG)
        static_assert(
            sizeof(GarbageCollector::AllocationData) == 160, // NOLINT
            "consider inserting new data");
#endif
    }

    GcAllocation::~GcAllocation() {
        // Get allocated data.
        const auto pAllocationInfo = getAllocationInfo();
        const auto pAllocatedObject = getAllocatedObject();

        // Call destructor on allocation info.
        pAllocationInfo->~GcAllocationInfo();

        // Call destructor on allocated object.
        pTypeInfo->getInvokeDestructor()(pAllocatedObject);

        // Free the allocated memory.
        ::operator delete(pAllocatedMemory);
    }

    GcTypeInfo* GcAllocation::getTypeInfo() const { return pTypeInfo; }

    void* GcAllocation::getAllocatedObject() const {
        return reinterpret_cast<char*>(pAllocatedMemory) + sizeof(GcAllocationInfo);
    }
}
