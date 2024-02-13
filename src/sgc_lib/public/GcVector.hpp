#pragma once

// Standard.
#include <vector>

// Custom.
#include "GcContainerBase.h"
#include "GarbageCollector.h"
#include "GcPtr.h"

namespace sgc {
    /**
     * `std::vector` wrapper for storing `GcPtr<InnerType>` items in a vector.
     *
     * @tparam OuterType `GcPtr`.
     * @tparam InnerType Type that `GcPtr`s of this container will store.
     */
    template <typename OuterType, typename InnerType = typename OuterType::value_type>
        requires std::same_as<OuterType, GcPtr<InnerType>> &&     // only GcPtr items are supported
                 (!std::derived_from<InnerType, GcContainerBase>) // inner containers not supported
    class GcVector : public GcContainerBase {
    public:
        virtual ~GcVector() override { notifyGarbageCollectorAboutDestruction(); }

        GcVector() : GcContainerBase(iterateOverGcPtrItems) {}

        /**
         * Constructs the container with the specified copies of elements with value.
         *
         * @param iCount Size of the vector.
         * @param value  Optional value to copy.
         */
        explicit GcVector(size_t iCount, const OuterType& value = OuterType())
            : GcContainerBase(iterateOverGcPtrItems), vData(iCount, value) {}

        /**
         * Adds the specified value to the container.
         *
         * @param valueToAdd Value to add to the container.
         */
        inline void push_back(const OuterType& valueToAdd) { // NOLINT: use name style as STL
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.push_back(valueToAdd);
        }

        /**
         * Adds the specified value to the container.
         *
         * @param valueToAdd Value to add to the container.
         */
        inline void push_back(OuterType&& valueToAdd) { // NOLINT: use name style as STL
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.push_back(std::forward<OuterType>(valueToAdd));
        }

        /**
         * Reserves storage.
         *
         * @param iSize New capacity of the vector, in number of elements.
         */
        inline void reserve(size_t iSize) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.reserve(iSize);
        }

        /** Reduces memory usage by freeing unused memory. */
        inline void shrink_to_fit() { // NOLINT: use name style as STL
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.shrink_to_fit();
        }

        /**
         * Returns the total number of elements in the container.
         *
         * @return Size.
         */
        constexpr size_t size() const noexcept { return vData.size(); }

        /**
         * Returns the number of elements that can be held in currently allocated storage.
         *
         * @return Capacity.
         */
        constexpr size_t capacity() const noexcept { return vData.capacity(); }

        /**
         * Checks whether the container is empty.
         *
         * @return `true` if empty, `false` otherwise.
         */
        constexpr bool empty() const noexcept { return vData.empty(); }

    private:
        /**
         * Iterates over items in @ref vData.
         *
         * @param pContainer  This.
         * @param onGcPtrItem Called on every GcPtr item in the container.
         */
        static inline void iterateOverGcPtrItems(
            const GcContainerBase* pContainer, const std::function<void(const GcPtrBase*)>& onGcPtrItem) {
            // Get this.
            const auto pThis = reinterpret_cast<const GcVector<OuterType>*>(pContainer);

            // Iterate over items.
            for (const auto& pGcPtr : pThis->vData) {
                onGcPtrItem(&pGcPtr);
            }
        }

        /** Actual array that stores GcPtr items. */
        std::vector<GcPtr<InnerType, false>> vData;
    };
}
