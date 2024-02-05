#pragma once

// Custom.
#include "GarbageCollector.h"
#include "GcTypeInfo.h"
#include "GcAllocation.h"

namespace sgc {
    class GcAllocationInfo;

    /** Base class for GC smart pointers, works similar to `std::shared_ptr`. */
    class GcPtrBase {
    public:
        GcPtrBase(const GcPtrBase&) = delete;
        GcPtrBase& operator=(const GcPtrBase&) = delete;

        virtual ~GcPtrBase();

    protected:
        GcPtrBase();

        /**
         * Allocates a new object of the specified type and registers it in the garbage collector
         * to be tracked if referenced by GC pointers or not.
         *
         * @param constructorArgs Arguments that will be passed to the type's constructor.
         *
         * @return Pointer to the allocated object of the user-specified type.
         */
        template <typename... ConstructorArgs>
        inline void* initializeFromNewAllocation(ConstructorArgs&&... constructorArgs) {
            // Make sure we are not running garbage collection while creating a new allocation.
            std::scoped_lock guard(GarbageCollector::get().mtxAllocationControlledData.first);

            // Create a new allocation (it's added to the GC "database" in allocation's constructor).
            const auto pNewAllocation = GcAllocation::registerNewAllocationWithInfo(
                std::forward<ConstructorArgs>(constructorArgs)...);

            // Save allocation info.
            pAllocationInfo = pNewAllocation->getAllocationInfo();

            return pNewAllocation->getAllocatedObject();
        }

        /**
         * Looks for an allocation info object near the specified pointer to the user object
         * and makes this GC pointer to point to a different GC allocation info.
         *
         * @warning If the pointer to the specified object was not previously created using `makeGc`
         * an error will be triggered.
         *
         * @param pUserObject Pointer to the object of the user-specified type.
         */
        void setNewAllocationInfoFromUserObject(void* pUserObject);

        /**
         * Returns info about the object that this pointer is pointing to.
         *
         * @return `nullptr` if this GC pointer is empty (just like a usual pointer), otherwise valid
         * allocation info.
         */
        GcAllocationInfo* getAllocationInfo() const;

    private:
        /**
         * Info about the object that this pointer is pointing to.
         *
         * @remark Can be `nullptr` if this GC pointer is empty (just like a usual pointer).
         */
        GcAllocationInfo* pAllocationInfo = nullptr;

        /**
         * Defines if this GC pointer object belongs to some other object as a field.
         *
         * @remark Initialized in constructor and never changed later.
         */
        bool bIsRootNode = false;
    };

    /** GC smart pointer for a specific type, works similar to `std::shared_ptr`. */
    template <typename Type> class GcPtr : public GcPtrBase {
        // `makeGc` function creates new GC pointer instances.
        template <typename ObjectType, typename... ConstructorArgs>
        friend inline GcPtr<ObjectType> makeGc(ConstructorArgs&&... args);

    public:
        virtual ~GcPtr() override = default;

        GcPtr(const GcPtr&) = delete;
        GcPtr& operator=(const GcPtr&) = delete;

        /**
         * Constructs a GC pointer from a raw pointer.
         *
         * @warning If the pointer to the specified target object was not previously created using `makeGc`
         * an error will be triggered.
         *
         * @param pTargetObject Object to pointer to.
         */
        explicit GcPtr(Type* pTargetObject) { updateInternalPointers(pTargetObject); }

        /**
         * Assignment operator from a raw pointer.
         *
         * @warning If the pointer to the specified target object was not previously created using `makeGc`
         * an error will be triggered.
         *
         * @param pTargetObject Object to pointer to.
         */
        inline void operator=(Type* pTargetObject) { updateInternalPointers(pTargetObject); }

    private:
        /**
         * Makes this GC pointer to point to a different object by updating internal pointers.
         *
         * @param pUserObject Pointer to the object of the user-specified type.
         */
        inline void updateInternalPointers(Type* pUserObject) {
            // Find and save allocation info object.
            setNewAllocationInfoFromUserObject(pUserObject);

#if defined(DEBUG)
            // Save pointer to the object for debugging.
            pDebugPtr = pUserObject;
#endif
        }

#if defined(DEBUG)
        /**
         * Object that this pointer is pointing to.
         *
         * @remark Only used for debugging purposes, to see the pointing object in debugger.
         */
        Type* pDebugPtr = nullptr;
#endif
    };

    /**
     * Allocates a new object of the specified type, similar to how `std::make_shared` works.
     *
     * @tparam constructorArgs Arguments that will be passed to the type's constructor.
     *
     * @return GC smart pointer to the allocated object.
     */
    template <typename Type, typename... ConstructorArgs>
    inline GcPtr<Type> makeGc(ConstructorArgs&&... constructorArgs) {
        // Create an empty GC pointer (notifies the GC to be added to the node graph).
        GcPtr<Type> pGcPtr;

        // Register a new allocation and set it to the pointer.
        const auto pObject =
            pGcPtr.initializeFromNewAllocation(std::forward<ConstructorArgs>(constructorArgs)...);

#if defined(DEBUG)
        // Save pointer to the object for debugging.
        pGcPtr.pDebugPtr = pObject;
#endif

        return pGcPtr;
    }
}
