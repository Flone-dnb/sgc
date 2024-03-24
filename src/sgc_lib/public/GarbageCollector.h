#pragma once

// Standard.
#include <mutex>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace sgc {
    class GcNode;
    class GcPtrBase;
    class GcContainerBase;
    class GcAllocation;
    struct GcAllocationInfo;

    /** Singleton that provides garbage management functionality. */
    class GarbageCollector {
        // GC pointers and GC containers notify garbage collector in constructor/destructor.
        friend class GcPtrBase;
        friend class GcContainerBase;

        // Modifies array of objects being constructed.
        friend class GcAllocationConstructionGuard;

        // Allocations add/remove themselves and their info objects.
        friend class GcAllocation;

    public:
        /** Groups various GC root nodes. */
        struct RootNodes {
            /** GcPtr nodes in the root set of the garbage collector's node graph. */
            std::unordered_set<const GcPtrBase*> gcPtrRootNodes;

            /** GcContainer nodes in the root set of the garbage collector's node graph. */
            std::unordered_set<const GcContainerBase*> gcContainerRootNodes;
        };

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

        /**
         * Runs garbage collection which might cause some no longer references objects to be destroyed.
         *
         * @return Number of user object (objects of the user-specified type) that were deleted (freed) during
         * the garbage collection.
         */
        size_t collectGarbage();

        /**
         * Returns the total number of existing (not deleted yet) allocations of user-specified types.
         *
         * @return Total number of user-objects alive.
         */
        size_t getAliveAllocationCount();

        /**
         * Returns pointer to read-only data of the garbage collector's internal root node set.
         *
         * @warning Do not delete (free) returned pointer or modify the root node set.
         *
         * @warning Only use with returned mutex.
         *
         * @remark Used for automated tests and debugging.
         *
         * @return Root nodes in the node graph.
         */
        std::pair<std::recursive_mutex, RootNodes>* getRootNodes();

        /**
         * Returns mutex that's locked while the garbage collector is running through GC node graph
         * (during garbage collection's first phase).
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Mutex.
         */
        std::recursive_mutex* getGcNodeGraphMutex();

    private:
        /** Groups mutex guarded data controlled by GC allocations. */
        struct AllocationData {
            /**
             * All not deleted (in-use) allocations allocated by the garbage collector.
             *
             * @remark GC allocation objects add themselves to this array in their constructor.
             */
            std::unordered_set<GcAllocation*> existingAllocations;

            /**
             * Used for quickly checking if some allocation info pointer is valid.
             *
             * @remark GC allocation objects add themselves to this array in their constructor.
             *
             * @remark Info objects here are owned by allocations from @ref
             * existingAllocations.
             */
            std::unordered_map<GcAllocationInfo*, GcAllocation*> allocationInfoRefs;
        };

        GarbageCollector();

        /**
         * Called by GC pointers or GC containers in their constructor to check that node (pointer or a
         * container) belongs to some object currently being created.
         *
         * @param pConstructedNode Constructed GC pointer or a GC container.
         *
         * @return `true` if this node was registered as a root node in the node graph and `false`
         * otherwise.
         */
        [[nodiscard]] bool onGcNodeConstructed(GcNode* pConstructedNode);

        /**
         * Called by root node GC pointers or containers in their destructor to update pending changes to
         * garbage collector.
         *
         * @param pRootNode GC pointer or a GC container that was a root node.
         */
        void onGcRootNodeBeingDestroyed(GcNode* pRootNode);

        /** Info about GC allocations. */
        std::pair<std::recursive_mutex, AllocationData> mtxAllocationData;

        /** Nodes in the root set of the garbage collector's node graph. */
        std::pair<std::recursive_mutex, RootNodes> mtxRootNodes;

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

        /**
         * Stores allocations that are about to be processed.
         *
         * @remark GC found pointers that point to allocations in the array but these allocations were
         * not scanned for inner GcPtr fields yet.
         */
        std::vector<GcAllocation*> vGrayAllocations;
    };
}
