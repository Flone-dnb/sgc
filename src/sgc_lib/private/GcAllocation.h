#pragma once

// Custom.
#include "GcAllocationInfo.hpp"
#include "GcAllocationConstructionGuard.h"
#include "GarbageCollector.h"
#include "GcTypeInfo.h"
#include "GcInfoCallbacks.hpp"

namespace sgc {
    /** Manages GC allocated object/memory. */
    class GcAllocation {
    public:
        GcAllocation() = delete;

        GcAllocation(const GcAllocation&) = delete;
        GcAllocation& operator=(const GcAllocation&) = delete;

        GcAllocation(GcAllocation&&) noexcept = delete;
        GcAllocation& operator=(GcAllocation&&) noexcept = delete;

        /**
         * Calls destructor on the previously allocated object and deletes (frees) all allocated memory.
         *
         * @remark Removes self and GC allocation info from the garbage collector's "database".
         */
        ~GcAllocation();

        /**
         * Allocates memory for a new GC controlled user object of the specified type and some memory
         * for the allocation info, then registers the new allocation in the garbage collector's "database".
         *
         * @warning You must call `delete` on the returned pointer (or on the same pointer in the garbage
         * collector's "database") when the allocation needs to be freed, it will free the memory for returned
         * GC allocation object, GC allocation info object and the user object.
         *
         * @remark Also calls constructor for the created object.
         *
         * @param constructorArgs Arguments that will be passed to the type's constructor.
         *
         * @return Newly allocated memory.
         */
        template <typename Type, typename... ConstructorArgs>
        static inline GcAllocation* registerNewAllocationWithInfo(ConstructorArgs&&... constructorArgs) {
            // Get type info.
            const auto pTypeInfo = GcTypeInfo::getStaticInfo<Type>();

            void* pAllocatedMemory = nullptr;
            try {
                // Allocate memory for the allocation info and the object.
                pAllocatedMemory = ::operator new(sizeof(GcAllocationInfo) + pTypeInfo->getTypeSize());
            } catch (std::exception& exception) {
                GcInfoCallbacks::getCriticalErrorCallback()(
                    "failed to allocate memory for a new GC controlled object");
                throw exception; // can't continue
            }

            // Create new allocation.
            auto pAllocation = new GcAllocation(pAllocatedMemory);

            // Call constructor on the allocation info
            // (using placement new operator to call constructor).
            new (pAllocation->getAllocationInfo()) GcAllocationInfo(pTypeInfo);

            {
                // Add this allocation as being constructed and remove by the end of the scope.
                GcAllocationConstructionGuard allocationGuard(pAllocation);

                // Invoke object constructor on the allocated object memory.
                new (pAllocation->getAllocatedObject())
                    Type(std::forward<ConstructorArgs>(constructorArgs)...);
            }

            // Subptrs are initialized (constructors of `GcPtr` objects register themselves).
            pTypeInfo->bAllSubPtrOffsetsInitialized = true;

            return pAllocation;
        }

        /**
         * Returns type information of the allocation.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Type info, always valid because points to a static variable.
         */
        GcTypeInfo* getTypeInfo() const;

        /**
         * Returns allocation info from @ref pAllocatedMemory.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Allocation info, always valid while this GC allocation object is alive.
         */
        GcAllocationInfo* getAllocationInfo() const;

        /**
         * Returns pointer to the allocated user object from @ref pAllocatedMemory.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Pointer to the allocated user object, always valid while this GC allocation object is
         * alive.
         */
        void* getAllocatedObject() const;

    private:
        /**
         * Allocates a new GC controlled object.
         *
         * @remark Adds self and GC allocation info to the garbage collector's "database".
         *
         * @param pAllocatedMemory Pointer to the allocated memory that stores allocation info and the
         * allocated object.
         */
        GcAllocation(void* pAllocatedMemory);

        /**
         * Pointer to the allocated memory that stores allocation info and the allocated object.
         *
         * @remark Initialized in constructor, always valid while this GC allocation object is alive.
         */
        void* const pAllocatedMemory = nullptr;
    };
}
