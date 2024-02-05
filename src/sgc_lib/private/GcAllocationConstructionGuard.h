#pragma once

namespace sgc {
    class GcAllocation;

    /**
     * RAII-style object used while calling allocated object constructor.
     *
     * Registers the specified allocation object in the array of currently constructing object
     * and unregisters it in destructor.
     */
    class GcAllocationConstructionGuard {
        // Can only be created by allocations.
        friend class GcAllocation;

    public:
        GcAllocationConstructionGuard() = delete;

        GcAllocationConstructionGuard(const GcAllocationConstructionGuard&) = delete;
        GcAllocationConstructionGuard& operator=(const GcAllocationConstructionGuard&) = delete;

        GcAllocationConstructionGuard(GcAllocationConstructionGuard&&) noexcept = delete;
        GcAllocationConstructionGuard& operator=(GcAllocationConstructionGuard&&) noexcept = delete;

        ~GcAllocationConstructionGuard();

    private:
        /**
         * Constructors a new object.
         *
         * @param pAllocation Allocation that uses this object.
         */
        GcAllocationConstructionGuard(GcAllocation* pAllocation);

        /** Allocation that uses this object. */
        GcAllocation* const pAllocation = nullptr;
    };
}
