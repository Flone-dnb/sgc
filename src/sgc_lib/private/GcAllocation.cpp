#include "GcAllocation.h"

// Custom.
#include "DebugLogger.hpp"

namespace sgc {

    GcAllocation::GcAllocation(void* pAllocatedMemory, GcTypeInfo* pTypeInfo)
        : pAllocatedMemory(pAllocatedMemory), pTypeInfo(pTypeInfo) {
        // Get allocations info.
        std::scoped_lock guard(GarbageCollector::get().mtxGcData.first);
        auto& mtxAllocationsInfo = GarbageCollector::get().mtxGcData.second.allocationData;

        SGC_DEBUG_LOG(std::format(
            "GcAllocation() with user object {} being constructed",
            reinterpret_cast<uintptr_t>(getAllocatedObject())));

        // Add self and allocation info.
        mtxAllocationsInfo.existingAllocations.insert(this);
        mtxAllocationsInfo.allocationInfoRefs[getAllocationInfo()] = this;
    }

    GcAllocation::~GcAllocation() {
        SGC_DEBUG_LOG(std::format(
            "GcAllocation() with user object {} being destroyed",
            reinterpret_cast<uintptr_t>(getAllocatedObject())));

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
