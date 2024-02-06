#pragma once

// Custom.
#include "GcAllocationColor.hpp"

namespace sgc {
    class GcTypeInfo;

    /**
     * Stores information needed for garbage collector about an allocated object.
     *
     * @remark In the memory stored right before allocated memory for the user-specified type, for example:
     * imagine flat memory: [...sizeof(GcAllocationInfo)sizeof(T)...]. This way our GC pointers can
     * operate on raw pointers but also keep track of GC allocation states (accessing allocation info by just
     * subtracting sizeof(GcAllocationInfo) from the raw pointer).
     */
    struct GcAllocationInfo {
        GcAllocationInfo() = default;

        /** Color of this allocation object. */
        GcAllocationColor color = GcAllocationColor::WHITE;
    };
}
