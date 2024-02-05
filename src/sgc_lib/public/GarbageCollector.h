#pragma once

// Standard.
#include <mutex>
#include <vector>
#include <unordered_set>

namespace sgc {
    class GcPtrBase;
    class GcAllocation;
    class GcAllocationInfo;

    /** Singleton that provides garbage management functionality. */
    class GarbageCollector {
        // GC pointers notify garbage collector in constructor.
        friend class GcPtrBase;

        // Modifies array of objects being constructed.
        friend class GcAllocationConstructionGuard;

        // Allocations add/remove themselves and their info objects.
        friend class GcAllocation;

    public:
        GarbageCollector(const GarbageCollector&) = delete;
        GarbageCollector& operator=(const GarbageCollector&) = delete;

        GarbageCollector(GarbageCollector&&) noexcept = delete;
        GarbageCollector& operator=(GarbageCollector&&) noexcept = delete;

        /**
         * Returns garbage collector singleton.
         *
         * @return Garbage collector singleton.
         */
        static GarbageCollector& get();

        /** Runs garbage collection which might cause some no longer references objects to be destroyed. */
        void collectGarbage();

    private:
        /**
         * Stores pending changes that occur between garbage collections and that need to be "applied"
         * to the data that GarbageCollector stores.
         *
         * @remark We don't apply changes to GC nodes immediately for several reasons, for example
         * when inside a garbage collection we delete (free) some object we might cause GC nodes
         * to be destroyed which might cause GC node graph to be modified (while we are iterating over it)
         * which is undesirable.
         */
        struct PendingNodeGraphChanges {
            /**
             * Destructed GC pointers that were root nodes, GC pointers add themselves to this array in their
             * destructor.
             *
             * @warning Pointers in this array point to deleted memory.
             */
            std::unordered_set<const GcPtrBase*> destroyedRootNodes;

            /** Newly created GC pointers that should be added to GC node graph as root nodes. */
            std::unordered_set<const GcPtrBase*> newRootNodes;
        };

        /** Groups mutex guarded data controlled by GC allocations. */
        struct AllocationControlledData {
            /**
             * All not deleted (in-use) allocations allocated by the garbage collector.
             *
             * @warning Delete (free) pointer from this array and it will erase itself
             * from this container and its allocation info from @ref allocationInfoRefs.
             *
             * @remark Modifications (addition/removal) of this array is done only by
             * GC allocation objects.
             */
            std::unordered_set<GcAllocation*> existingAllocations;

            /**
             * Used for quickly checking if some allocation info pointer is valid.
             *
             * @warning Do not delete (free) pointers from this container, they will be
             * erased and deleted by GC allocations.
             *
             * @remark Modifications (addition/removal) of this array is done only by
             * GC allocation objects.
             *
             * @remark Info objects here are owned by allocations from @ref
             * existingAllocations.
             */
            std::unordered_set<GcAllocationInfo*> allocationInfoRefs;
        };

        GarbageCollector() = default;

        /**
         * Called by GC pointers in their constructor to check that pointer belongs to some object
         * currently being created.
         *
         * @param pConstructedPtr Constructed GC pointer.
         *
         * @return `true` if this pointer is registered as a root node in the node graph and `false`
         * otherwise.
         */
        [[nodiscard]] bool onGcPointerConstructed(GcPtrBase* pConstructedPtr);

        /** Pending changes to the node graph. */
        std::pair<std::mutex, PendingNodeGraphChanges> mtxPendingNodeGraphChanges;

        /** Data controlled by GC allocations. */
        std::pair<std::recursive_mutex, AllocationControlledData> mtxAllocationControlledData;

        /** Nodes in the root set of the garbage collector's node graph. */
        std::pair<std::recursive_mutex, std::unordered_set<const GcPtrBase*>> mtxRootNodes;

        /**
         * Stores GC allocated objects currently being constructed (allocated).
         *
         * @remark When `makeGc` is used we allocate a new GC allocation object and add it here,
         * this causes a new object (of the user-specific type) to be allocated and constructed. Construction
         * of that new object causes all of its fields to be constructed and thus constructors of our GC
         * pointer types are triggered (if the user-specified type has them). In order to understand which GC
         * pointers belongs to which object (in order to construct GC node graph relations) constructors of
         * our GC pointer types refer to this array to check if their location in the memory belongs to the
         * memory range of the newly allocated object.
         *
         * @remark Using `vector` because order in this array is important. If the user creates a new object
         * of some type `Foo` using `makeGc` and in constructor of this type `Foo` user-code does another
         * `makeGc` this will cause us to add another (second) GC allocation to this array and all GC pointer
         * constructors will need to reference this second (last) GC allocation object.
         *
         * @remark Must be used with the mutex.
         */
        std::pair<std::recursive_mutex, std::vector<GcAllocation*>> mtxCurrentlyConstructingObjects;
    };
}
