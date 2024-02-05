#pragma once

// Custom.
#include "GcAllocationColor.hpp"

namespace sgc {
    class GcTypeInfo;

    /**
     * Stores information about GC allocated object.
     *
     * @remark In the memory stored right before allocated memory for the user-specified type, for example:
     * imagine flat memory: [...sizeof(GcAllocationInfo)sizeof(T)...]. This way our GC pointers can
     * operate on raw pointers but also keep track of GC allocation states (accessing allocation info by just
     * subtracting sizeof(GcAllocationInfo) from the raw pointer).
     */
    class GcAllocationInfo {
    public:
        GcAllocationInfo() = delete;

        GcAllocationInfo(const GcAllocationInfo&) = delete;
        GcAllocationInfo& operator=(const GcAllocationInfo&) = delete;

        GcAllocationInfo(GcAllocationInfo&&) noexcept = delete;
        GcAllocationInfo& operator=(GcAllocationInfo&&) noexcept = delete;

        /**
         * Constructs a new allocation info.
         *
         * @param pTypeInfo Type of the allocation.
         */
        GcAllocationInfo(GcTypeInfo* pTypeInfo) : pTypeInfo(pTypeInfo) {}

        /** Color of this allocation object. */
        GcAllocationColor color = GcAllocationColor::WHITE;

        /**
         * Pointer to static type info object.
         *
         * @remark Initialized in constructor, always valid since it points to a static variable.
         */
        GcTypeInfo* const pTypeInfo = nullptr;
    };
}
