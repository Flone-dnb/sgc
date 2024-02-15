#pragma once

// Custom.
#include "GarbageCollector.h"
#include "GcTypeInfo.h"
#include "GcAllocation.h"
#include "GcNode.hpp"

namespace sgc {
    class GcAllocation;
    struct GcAllocationInfo;

    /** Base class for GC smart pointers. */
    class GcPtrBase : public GcNode {
        // Garbage collector inspects referenced allocation.
        friend class GarbageCollector;

    public:
        GcPtrBase() = delete;

        GcPtrBase(const GcPtrBase&) = delete;
        GcPtrBase& operator=(const GcPtrBase&) = delete;

        virtual ~GcPtrBase() override;

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
        /**
         * Constructor.
         *
         * @param bCanBeRootNode `true` if this pointer can be a root node in the GC graph, `false`
         * otherwise.
         */
        explicit GcPtrBase(bool bCanBeRootNode);

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
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            // Create a new allocation (it's added to the GC "database" in allocation's constructor).
            pAllocation = GcAllocation::registerNewAllocationWithInfo<Type>(
                std::forward<ConstructorArgs>(constructorArgs)...);

            return pAllocation->getAllocatedObject();
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
        void setAllocationFromUserObject(void* pUserObject);

    private:
        /**
         * Allocation that this pointer is pointing to.
         *
         * @remark Can be `nullptr` if this GC pointer is empty (just like a usual pointer).
         */
        GcAllocation* pAllocation = nullptr;
    };

    /**
     * GC smart pointer for a specific type, works similar to `std::shared_ptr`.
     *
     * @tparam Type           Type of the object that the pointer will hold.
     * @tparam bCanBeRootNode Used internally, please use the default value (`true`). Determines
     * if this GcPtr can be a root node in the GC graph.
     */
    template <typename Type, bool bCanBeRootNode = true> class GcPtr : public GcPtrBase {
        // `makeGc` function creates new GC pointer instances.
        template <typename ObjectType, typename... ConstructorArgs>
        friend inline GcPtr<ObjectType> makeGc(ConstructorArgs&&... args);

        // Allow other GC pointers to look into our internals.
        template <typename OtherType, bool> friend class GcPtr;

    public:
        /** Used by GC containers. */
        using value_type = Type;

        virtual ~GcPtr() override = default;

        // ----------------------------------------------------------------------------------------
        //                                 CONSTRUCTORS
        // ----------------------------------------------------------------------------------------

        /** Constructs an empty (`nullptr`) pointer. */
        GcPtr() : GcPtrBase(bCanBeRootNode) {}

        /**
         * Constructs a GC pointer from a raw pointer.
         *
         * @warning If the pointer to the specified target object was not previously created using `makeGc`
         * an error will be triggered.
         *
         * @param pTargetObject Object to pointer to.
         */
        explicit GcPtr(Type* pTargetObject) : GcPtrBase(bCanBeRootNode) {
            updateInternalPointers(pTargetObject);
        }

        /**
         * Constructs a GC pointer from another GC pointer.
         *
         * @param pOther GC pointer to copy.
         */
        GcPtr(const GcPtr<Type, bCanBeRootNode>& pOther) : GcPtrBase(bCanBeRootNode) {
            updateInternalPointers(pOther.get());
        }

        /**
         * Constructs a GC pointer from another GC pointer.
         *
         * @param pOther GC pointer to move.
         */
        GcPtr(GcPtr<Type, bCanBeRootNode>&& pOther) noexcept : GcPtrBase(bCanBeRootNode) {
            *this = std::move(pOther);
        };

        /**
         * Constructs a GC pointer from another GC pointer.
         *
         * @param pOther GC pointer to copy.
         */
        template <bool bOther> GcPtr(const GcPtr<Type, bOther>& pOther) : GcPtrBase(bCanBeRootNode) {
            updateInternalPointers(pOther.get());
        }

        /**
         * Constructs a GC pointer from another GC pointer.
         *
         * @param pOther GC pointer to move.
         */
        template <bool bOther> GcPtr(GcPtr<Type, bOther>&& pOther) noexcept : GcPtrBase(bCanBeRootNode) {
            *this = std::move(pOther);
        };

        /**
         * Constructs a GC pointer from another GC pointer.
         *
         * @param pOther GC pointer to copy.
         */
        template <typename ChildType>
            requires std::derived_from<ChildType, Type> && (!std::same_as<ChildType, Type>)
        GcPtr(const GcPtr<ChildType, bCanBeRootNode>& pOther) : GcPtrBase(bCanBeRootNode) {
            updateInternalPointers(pOther.get());
        }

        /**
         * Constructs a GC pointer from another GC pointer.
         *
         * @param pOther GC pointer to move.
         */
        template <typename ChildType>
            requires std::derived_from<ChildType, Type> && (!std::same_as<ChildType, Type>)
        GcPtr(GcPtr<ChildType, bCanBeRootNode>&& pOther) noexcept : GcPtrBase(bCanBeRootNode) {
            *this = std::move(pOther);
        };

        /**
         * Constructs a GC pointer from another GC pointer.
         *
         * @param pOther GC pointer to copy.
         */
        template <typename ChildType, bool bOther>
            requires std::derived_from<ChildType, Type> && (!std::same_as<ChildType, Type>)
        GcPtr(const GcPtr<ChildType, bOther>& pOther) : GcPtrBase(bCanBeRootNode) {
            updateInternalPointers(pOther.get());
        }

        /**
         * Constructs a GC pointer from another GC pointer.
         *
         * @param pOther GC pointer to move.
         */
        template <typename ChildType, bool bOther>
            requires std::derived_from<ChildType, Type> && (!std::same_as<ChildType, Type>)
        GcPtr(GcPtr<ChildType, bOther>&& pOther) noexcept : GcPtrBase(bCanBeRootNode) {
            *this = std::move(pOther);
        };

        // ----------------------------------------------------------------------------------------
        //                                 OPERATOR =
        // ----------------------------------------------------------------------------------------

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
            if (this == &pOther) {
                return *this;
            }

            // "Move" data into self.
            updateInternalPointers(pOther.get());

            // Clear moved object.
            pOther.updateInternalPointers(nullptr);

            return *this;
        };

        /**
         * Copy assignment operator from another GC pointer.
         *
         * @param pOther GC pointer to copy.
         *
         * @return This.
         */
        template <bool bOther> GcPtr& operator=(const GcPtr<Type, bOther>& pOther) {
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
        template <bool bOther> GcPtr& operator=(GcPtr<Type, bOther>&& pOther) noexcept {
            if (reinterpret_cast<void*>(this) == reinterpret_cast<void*>(&pOther)) {
                return *this;
            }

            // "Move" data into self.
            updateInternalPointers(pOther.get());

            // Clear moved object.
            pOther.updateInternalPointers(nullptr);

            return *this;
        };

        /**
         * Copy assignment operator from another GC pointer.
         *
         * @param pOther GC pointer to copy.
         *
         * @return This.
         */
        template <typename ChildType>
            requires std::derived_from<ChildType, Type> && (!std::same_as<ChildType, Type>)
        GcPtr& operator=(const GcPtr<ChildType>& pOther) {
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
        template <typename ChildType>
            requires std::derived_from<ChildType, Type> && (!std::same_as<ChildType, Type>)
        GcPtr& operator=(GcPtr<ChildType>&& pOther) noexcept {
            // Don't move self to self.
            if (reinterpret_cast<void*>(this) == reinterpret_cast<void*>(&pOther)) {
                return *this;
            }

            // "Move" data into self.
            updateInternalPointers(pOther.get());

            // Clear moved object.
            pOther.updateInternalPointers(nullptr);

            return *this;
        };

        /**
         * Copy assignment operator from another GC pointer.
         *
         * @param pOther GC pointer to copy.
         *
         * @return This.
         */
        template <typename ChildType, bool bOther>
            requires std::derived_from<ChildType, Type> && (!std::same_as<ChildType, Type>)
        GcPtr& operator=(const GcPtr<ChildType, bOther>& pOther) {
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
        template <typename ChildType, bool bOther>
            requires std::derived_from<ChildType, Type> && (!std::same_as<ChildType, Type>)
        GcPtr& operator=(GcPtr<ChildType, bOther>&& pOther) noexcept {
            // Don't move self to self.
            if (this == &pOther) {
                return *this;
            }

            // "Move" data into self.
            updateInternalPointers(pOther.get());

            // Clear moved object.
            pOther.updateInternalPointers(nullptr);

            return *this;
        };

        // ----------------------------------------------------------------------------------------
        //                                 OPERATOR ==
        // ----------------------------------------------------------------------------------------

        /**
         * Tests if this GC pointer points to the same user object as the specified other GC pointer.
         *
         * @param pOther Raw pointer to compare with.
         *
         * @return `true` if both GC pointers point to the same object of the user-specified type,
         * `false` if pointers are different.
         */
        bool operator==(Type* pOther) const { return getUserObject() == pOther; }

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
         * Tests if this GC pointer points to the same user object as the specified other GC pointer.
         *
         * @param pOther Other GC pointer.
         *
         * @return `true` if both GC pointers point to the same object of the user-specified type,
         * `false` if pointers are different.
         */
        template <bool bOther> bool operator==(const GcPtr<Type, bOther>& pOther) const {
            return getUserObject() == pOther.getUserObject();
        }

        /**
         * Tests if this GC pointer points to the same user object as the specified other GC pointer.
         *
         * @param pOther Other GC pointer.
         *
         * @return `true` if both GC pointers point to the same object of the user-specified type,
         * `false` if pointers are different.
         */
        template <typename ChildType>
            requires std::derived_from<ChildType, Type> && (!std::same_as<ChildType, Type>)
        bool operator==(const GcPtr<ChildType>& pOther) const {
            return getUserObject() == pOther.getUserObject();
        }

        /**
         * Tests if this GC pointer points to the same user object as the specified other GC pointer.
         *
         * @param pOther Other GC pointer.
         *
         * @return `true` if both GC pointers point to the same object of the user-specified type,
         * `false` if pointers are different.
         */
        template <typename ChildType, bool bOther>
            requires std::derived_from<ChildType, Type> && (!std::same_as<ChildType, Type>)
        bool operator==(const GcPtr<ChildType, bOther>& pOther) const {
            return getUserObject() == pOther.getUserObject();
        }

        // ----------------------------------------------------------------------------------------
        //                                 OTHER
        // ----------------------------------------------------------------------------------------

        /**
         * Member access operator.
         *
         * @return `nullptr` if this GC pointer is empty, otherwise pointer to the object of the
         * user-specified type that this pointer is pointing to.
         */
        Type* operator->() const { return get(); }

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
            setAllocationFromUserObject(pUserObject);

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
