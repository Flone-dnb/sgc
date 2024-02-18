#pragma once

// Standard.
#include <vector>

namespace sgc {
    class GcAllocation;
    class GcTypeInfo;
    class GcNode;

    /** Stores information about a specific GC controlled type. */
    class GcTypeInfo {
        // Only garbage collector can register new GcPtr field offsets.
        friend class GarbageCollector;

        // Allocation marks GcPtr field offsets as initialized after an object of this type has finished its
        // constructor.
        friend class GcAllocation;

    public:
        /**
         * Type used to store offsets from GC controlled type (class/struct) start to GC node (GC pointer or a
         * GC container) fields of the type.
         */
        using gcnode_field_offset_t = unsigned int;

        /** Signature of the function to invoke destructor. */
        using GcTypeInfoInvokeDestructor = void (*)(void* pObjectMemory) noexcept;

        GcTypeInfo() = delete;

        GcTypeInfo(const GcTypeInfo&) = delete;
        GcTypeInfo& operator=(const GcTypeInfo&) = delete;

        GcTypeInfo(GcTypeInfo&&) noexcept = delete;
        GcTypeInfo& operator=(GcTypeInfo&&) noexcept = delete;

        /**
         * Constructs a new type info.
         *
         * @param iTypeSize          Size of the type in bytes.
         * @param pInvokeDestructor  Pointer to type's destructor.
         */
        GcTypeInfo(size_t iTypeSize, GcTypeInfoInvokeDestructor pInvokeDestructor);

        /**
         * Returns static type information.
         *
         * @return Pointer to static variable.
         */
        template <typename Type> static inline GcTypeInfo* getStaticInfo() {
            return &GcTypeInfoStatic<Type>::info;
        }

        /**
         * Returns size of the type in bytes.
         *
         * @return Size in bytes.
         */
        size_t getTypeSize() const;

        /**
         * Returns pointer to to function to invoke type's destructor.
         *
         * @return Function pointer.
         */
        GcTypeInfoInvokeDestructor getInvokeDestructor() const;

        /**
         * Returns offsets from type start to GC pointer fields.
         *
         * @remark Used for automated tests.
         *
         * @return Offsets.
         */
        const std::vector<gcnode_field_offset_t>& getGcPtrFieldOffsets();

    private:
        /** Static "accessor" for GC controlled type information. */
        template <typename Type> struct GcTypeInfoStatic {
            /**
             * Invokes type's destructor on the specified GC allocated memory.
             *
             * @param pObjectMemory Allocated memory for the object.
             */
            static void invokeDestructor(void* pObjectMemory) noexcept {
                // Change pointer type.
                auto pObject = reinterpret_cast<Type*>(pObjectMemory);

                // Call destructor.
                pObject->~Type();
            }

            /** Type information. */
            static GcTypeInfo info;
        };

        /**
         * Checks if the specified pointer belongs to the memory region of the object of the specified
         * allocation and saves pointer's offset from type start.
         *
         * @param pConstructedNode Newly constructed GC node (maybe a field in some object).
         * @param pAllocation      Allocation that may be the owner object of that pointer.
         *
         * @return `true` if the specified pointer belongs to the specified allocation (and pointer's offset
         * was possibly registered), `false` if the specified pointer does not belong to the memory region of
         * the specified allocation.
         */
        bool tryRegisteringGcNodeFieldOffset(GcNode* pConstructedNode, GcAllocation* pAllocation);

        /**
         * Offsets from GC controlled type start to each field that has a GC pointer type.
         *
         * @remark Might not contain all offsets until @ref bAllGcNodeFieldOffsetsInitialized is `true`.
         */
        std::vector<gcnode_field_offset_t> vGcPtrFieldOffsets;

        /**
         * Offsets from GC controlled type start to each field that has a GC container type.
         *
         * @remark Might not contain all offsets until @ref bAllGcNodeFieldOffsetsInitialized is `true`.
         */
        std::vector<gcnode_field_offset_t> vGcContainerFieldOffsets;

        /**
         * `true` if @ref vGcPtrFieldOffsets and @ref vGcContainerFieldOffsets are fully initialized and all
         * offsets were added, `false` if the type information is still being gathered.
         */
        bool bAllGcNodeFieldOffsetsInitialized = false;

        /** Pointer to the function to invoke type's destructor. */
        GcTypeInfoInvokeDestructor const pInvokeDestructor = nullptr;

        /** Size in bytes of the type. */
        size_t const iTypeSize = 0;
    };

    /** Initializer for static type info. */
    template <typename T>
    GcTypeInfo GcTypeInfo::GcTypeInfoStatic<T>::info{
        sizeof(T),
        GcTypeInfoStatic<T>::invokeDestructor,
    };
}
