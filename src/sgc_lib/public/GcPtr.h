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

        /**
         * Returns info about the object that this pointer is pointing to.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return `nullptr` if this GC pointer is empty (just like a usual pointer), otherwise valid
         * allocation info.
         */
        GcAllocationInfo* getAllocationInfo() const;

        /**
         * Returns pointer to the object of the user-specified type that this pointer is pointing to.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return `nullptr` if this GC pointer is empty (just like a usual pointer), otherwise valid
         * pointer.
         */
        void* getUserObject() const;

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
        template <typename Type, typename... ConstructorArgs>
        inline void* initializeFromNewAllocation(ConstructorArgs&&... constructorArgs) {
            // Make sure we are not running a garbage collection while creating a new allocation.
            std::scoped_lock guard(GarbageCollector::get().mtxAllocationControlledData.first);

            // Create a new allocation (it's added to the GC "database" in allocation's constructor).
            const auto pNewAllocation = GcAllocation::registerNewAllocationWithInfo<Type>(
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
         * @param pUserObject Pointer to the object of the user-specified type,
         * if `nullptr` then internal pointer is just set to `nullptr`.
         */
        void setNewAllocationInfoFromUserObject(void* pUserObject);

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
    template <typename Type> class GcPtr : protected GcPtrBase {
        // `makeGc` function creates new GC pointer instances.
        template <typename ObjectType, typename... ConstructorArgs>
        friend inline GcPtr<ObjectType> makeGc(ConstructorArgs&&... args);

    public:
        /** Constructs an empty (`nullptr`) pointer. */
        GcPtr() = default;

        virtual ~GcPtr() override = default;

        /**
         * Constructs a GC pointer from another GC pointer.
         *
         * @param pOther GC pointer to copy.
         */
        GcPtr(const GcPtr<Type>& pOther) { updateInternalPointers(pOther.get()); };

        /**
         * Constructs a GC pointer from another GC pointer.
         *
         * @param pOther GC pointer to move.
         */
        GcPtr(GcPtr<Type>&& pOther) noexcept {
            // "Move" data into self.
            updateInternalPointers(pOther.get());

            // Clear moved object.
            pOther.updateInternalPointers(nullptr);
        };

        /**
         * Copy assignment operator from another GC pointer.
         *
         * @param pOther GC pointer to copy.
         *
         * @return This.
         */
        GcPtr& operator=(const GcPtr& pOther) {
            updateInternalPointers(pOther.get());
            return *this;
        };

        /**
         * Move assignment operator from another GC pointer.
         *
         * @param pOther GC pointer to move.
         *
         * @return This.
         */
        GcPtr& operator=(GcPtr&& pOther) noexcept {
            // "Move" data into self.
            updateInternalPointers(pOther.get());

            // Clear moved object.
            pOther.updateInternalPointers(nullptr);

            return *this;
        };

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

        /**
         * Tests if this GC pointer points to the same user object as the specified other GC pointer.
         *
         * @param pOther Other GC pointer.
         *
         * @return `true` if both GC pointers point to the same object of the user-specified type,
         * `false` if pointers are different.
         */
        bool operator==(const GcPtr& pOther) const { return getUserObject() == pOther.getUserObject(); }

        /**
         * Returns pointer to the object of the user-specified type that this pointer is pointing to.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return `nullptr` if this GC pointer is empty (just like a usual pointer), otherwise valid
         * pointer.
         */
        Type* get() const { return reinterpret_cast<Type*>(getUserObject()); }

    private:
        /**
         * Makes this GC pointer to point to a different object by updating internal pointers.
         *
         * @param pUserObject Pointer to the object of the user-specified type,
         * if `nullptr` then ignored.
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
            pGcPtr.initializeFromNewAllocation<Type>(std::forward<ConstructorArgs>(constructorArgs)...);

#if defined(DEBUG)
        // Save pointer to the object for debugging.
        pGcPtr.pDebugPtr = reinterpret_cast<Type*>(pObject);
#endif

        return pGcPtr;
    }
}
