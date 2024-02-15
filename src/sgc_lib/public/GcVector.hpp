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
        requires(std::same_as<OuterType, GcPtr<InnerType, true>> ||   // only GcPtr items are supported
                 std::same_as<OuterType, GcPtr<InnerType, false>>) && //
                (!std::derived_from<InnerType, GcContainerBase>)      // inner containers not supported
    class GcVector : public GcContainerBase {
        // Allow other container to look into our internals.
        template <typename SomeOuterType, typename SomeInnerType>
            requires(std::same_as<SomeOuterType, GcPtr<SomeInnerType, true>> ||
                     std::same_as<SomeOuterType, GcPtr<SomeInnerType, false>>) &&
                    (!std::derived_from<SomeInnerType, GcContainerBase>)
        friend class GcVector;

    public:
        /** Type that we store in `std::vector`. */
        using vec_item_t = sgc::GcPtr<InnerType, false>;

        virtual ~GcVector() override { notifyGarbageCollectorAboutDestruction(); }

        /** Creates an empty container. */
        GcVector() : GcContainerBase(iterateOverGcPtrItems) {}

        /**
         * Copy constructor.
         *
         * @param vOther Container to copy.
         */
        GcVector(const GcVector& vOther) : GcContainerBase(iterateOverGcPtrItems) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData = vOther.vData;
        }

        /**
         * Move constructor.
         *
         * @param vOther Container to move.
         */
        GcVector(GcVector&& vOther) noexcept : GcContainerBase(iterateOverGcPtrItems) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData = std::move(vOther.vData);
        }

        /**
         * Constructs the container with the specified copies of elements with value.
         *
         * @param iCount Size of the vector.
         * @param value  Optional value to copy.
         */
        explicit GcVector(size_t iCount, const vec_item_t& value = vec_item_t())
            : GcContainerBase(iterateOverGcPtrItems) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData = std::vector<vec_item_t>(iCount, value);
        }

        /**
         * Copy assignment operator.
         *
         * @param vOther Container to copy.
         *
         * @return This.
         */
        GcVector& operator=(const GcVector& vOther) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData = vOther.vData;

            return *this;
        }

        /**
         * Move assignment operator.
         *
         * @param vOther Container to move.
         *
         * @return This.
         */
        GcVector& operator=(GcVector&& vOther) noexcept {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData = std::move(vOther.vData);

            return *this;
        }

        /**
         * Comparison operator.
         *
         * @param other Vector which contents to compare with.
         *
         * @return `true` if containers are equal, `false` if different.
         */
        constexpr bool operator==(const GcVector& other) const noexcept { return vData == other.vData; }

        /**
         * Returns a reference to the element at specified location, with bounds checking.
         *
         * @param iPos Position of the element to return.
         *
         * @return Reference to the requested element.
         */
        constexpr vec_item_t& at(size_t iPos) { return vData.at(iPos); }

        /**
         * Returns a reference to the element at specified location. No bounds checking is performed.
         *
         * @param iPos Position of the element to return.
         *
         * @return Reference to the requested element.
         */
        constexpr vec_item_t& operator[](size_t iPos) { return vData[iPos]; }

        /**
         * Returns a reference to the first element in the container.
         *
         * @warning Calling front on an empty container causes undefined behavior.
         *
         * @return Reference to the first element.
         */
        constexpr vec_item_t& front() { return vData.front(); }

        /**
         * Returns a reference to the last element in the container.
         *
         * @warning Calling back on an empty container causes undefined behavior.
         *
         * @return Reference to the last element.
         */
        constexpr vec_item_t& back() { return vData.back(); }

        /**
         * Returns a reference to the first element in the container.
         *
         * @warning Calling front on an empty container causes undefined behavior.
         *
         * @return Reference to the first element.
         */
        constexpr vec_item_t& front() const { return vData.front(); }

        /**
         * Returns a reference to the last element in the container.
         *
         * @warning Calling back on an empty container causes undefined behavior.
         *
         * @return Reference to the last element.
         */
        constexpr vec_item_t& back() const { return vData.back(); }

        /**
         * Returns pointer to the underlying array serving as element storage.
         *
         * @return Pointer to the underlying element storage.
         */
        constexpr vec_item_t* data() noexcept { return vData.data(); }

        /**
         * Returns an iterator to the first element of the vector.
         *
         * @remark If the vector is empty, the returned iterator will be equal to @ref end.
         *
         * @return Iterator to the first element.
         */
        constexpr std::vector<vec_item_t>::iterator begin() noexcept { return vData.begin(); }

        /**
         * Returns an iterator to the element following the last element of the vector.
         *
         * @warning This element acts as a placeholder, attempting to access it results in undefined behavior.
         *
         * @return Iterator to the element following the last element.
         */
        constexpr std::vector<vec_item_t>::iterator end() noexcept { return vData.end(); }

        /**
         * Returns an iterator to the first element of the vector.
         *
         * @remark If the vector is empty, the returned iterator will be equal to @ref end.
         *
         * @return Iterator to the first element.
         */
        constexpr std::vector<vec_item_t>::const_iterator cbegin() const noexcept { return vData.cbegin(); }

        /**
         * Returns an iterator to the element following the last element of the vector.
         *
         * @warning This element acts as a placeholder, attempting to access it results in undefined behavior.
         *
         * @return Iterator to the element following the last element.
         */
        constexpr std::vector<vec_item_t>::const_iterator cend() const noexcept { return vData.cend(); }

        /**
         * Returns a reverse iterator to the first element of the reversed vector.
         *
         * @remark If the vector is empty, the returned iterator will be equal to @ref end.
         *
         * @return Reverse iterator to the first element.
         */
        constexpr std::vector<vec_item_t>::iterator rbegin() noexcept { return vData.rbegin(); }

        /**
         * Returns a reverse iterator to the element following the last element of the reversed vector.
         *
         * @warning This element acts as a placeholder, attempting to access it results in undefined behavior.
         *
         * @return Reverse iterator to the element following the last element.
         */
        constexpr std::vector<vec_item_t>::iterator rend() noexcept { return vData.rend(); }

        /**
         * Returns a reverse iterator to the first element of the reversed vector.
         *
         * @remark If the vector is empty, the returned iterator will be equal to @ref end.
         *
         * @return Reverse iterator to the first element.
         */
        constexpr std::vector<vec_item_t>::iterator crbegin() const noexcept { return vData.crbegin(); }

        /**
         * Returns a reverse iterator to the element following the last element of the reversed vector.
         *
         * @warning This element acts as a placeholder, attempting to access it results in undefined behavior.
         *
         * @return Reverse iterator to the element following the last element.
         */
        constexpr std::vector<vec_item_t>::iterator crend() const noexcept { return vData.crend(); }

        /**
         * Checks whether the container is empty.
         *
         * @return `true` if empty, `false` otherwise.
         */
        constexpr bool empty() const noexcept { return vData.empty(); }

        /**
         * Returns the total number of elements in the container.
         *
         * @return Size.
         */
        constexpr size_t size() const noexcept { return vData.size(); }

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

        /**
         * Returns the number of elements that can be held in currently allocated storage.
         *
         * @return Capacity.
         */
        constexpr size_t capacity() const noexcept { return vData.capacity(); }

        /** Reduces memory usage by freeing unused memory. */
        inline void shrink_to_fit() { // NOLINT: use name style as STL
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.shrink_to_fit();
        }

        /** Erases all elements from the container. */
        inline void clear() {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.clear();
        }

        /**
         * Inserts elements at the specified location in the container.
         *
         * @param pos   Iterator before which the content will be inserted.
         * @param value Value to insert.
         */
        inline void insert(std::vector<vec_item_t>::iterator pos, const vec_item_t& value) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.insert(pos, value);
        }

        /**
         * Inserts elements at the specified location in the container.
         *
         * @param pos   Iterator before which the content will be inserted.
         * @param value Value to insert.
         */
        inline void insert(std::vector<vec_item_t>::iterator pos, vec_item_t&& value) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.insert(pos, std::forward<vec_item_t>(value));
        }

        /**
         * Inserts elements at the specified location in the container.
         *
         * @param pos   Iterator before which the content will be inserted.
         * @param value Value to insert.
         */
        inline void insert(std::vector<vec_item_t>::const_iterator pos, const vec_item_t& value) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.insert(pos, value);
        }

        /**
         * Inserts elements at the specified location in the container.
         *
         * @param pos   Iterator before which the content will be inserted.
         * @param value Value to insert.
         */
        inline void insert(std::vector<vec_item_t>::const_iterator pos, vec_item_t&& value) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.insert(pos, std::forward<vec_item_t>(value));
        }

        /**
         * Erases the specified elements from the container.
         *
         * @param pos Iterator to the element to remove.
         *
         * @return Iterator following the last removed element.
         */
        inline std::vector<vec_item_t>::iterator erase(std::vector<vec_item_t>::iterator pos) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            return vData.erase(pos);
        }

        /**
         * Erases the specified elements from the container.
         *
         * @param pos Iterator to the element to remove.
         *
         * @return Iterator following the last removed element.
         */
        inline std::vector<vec_item_t>::iterator erase(std::vector<vec_item_t>::const_iterator pos) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            return vData.erase(pos);
        }

        /**
         * Erases the specified elements from the container.
         *
         * @param first Range of elements to remove.
         * @param last  Range of elements to remove.
         *
         * @return Iterator following the last removed element.
         */
        inline std::vector<vec_item_t>::iterator
        erase(std::vector<vec_item_t>::iterator first, std::vector<vec_item_t>::iterator last) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            return vData.erase(first, last);
        }

        /**
         * Erases the specified elements from the container.
         *
         * @param first Range of elements to remove.
         * @param last  Range of elements to remove.
         *
         * @return Iterator following the last removed element.
         */
        inline std::vector<vec_item_t>::iterator
        erase(std::vector<vec_item_t>::const_iterator first, std::vector<vec_item_t>::const_iterator last) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            return vData.erase(first, last);
        }

        /**
         * Adds the specified value to the container.
         *
         * @param valueToAdd Value to add to the container.
         */
        inline void push_back(const vec_item_t& valueToAdd) { // NOLINT: use name style as STL
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.push_back(valueToAdd);
        }

        /**
         * Adds the specified value to the container.
         *
         * @param valueToAdd Value to add to the container.
         */
        inline void push_back(vec_item_t&& valueToAdd) { // NOLINT: use name style as STL
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.push_back(std::forward<vec_item_t>(valueToAdd));
        }

        /**
         * Appends a new element to the end of the container.
         *
         * @param args Arguments to forward to the constructor of the element.
         *
         * @return A reference to the inserted element.
         */
        template <class... Args>
        inline vec_item_t& emplace_back(Args&&... args) { // NOLINT: use name style as STL
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            return vData.emplace_back(std::forward<Args>(args)...);
        }

        /** Removes the last element of the container. */
        inline void pop_back() { // NOLINT: use name style as STL
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.pop_back();
        }

        /**
         * Resizes the container to contain count elements.
         *
         * @param iCount New size of the container.
         */
        inline void resize(size_t iCount) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.resize(iCount);
        }

        /**
         * Resizes the container to contain count elements.
         *
         * @param iCount New size of the container.
         * @param value  The value to initialize the new elements with.
         */
        inline void resize(size_t iCount, const vec_item_t& value) {
            // Make sure the GC is not currently iterating over this container since we modify the container.
            std::scoped_lock guard(*GarbageCollector::get().getGarbageCollectionMutex());

            vData.resize(iCount, value);
        }

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
            using item_t = GcVector<OuterType>::vec_item_t;
            const auto pThis = reinterpret_cast<const GcVector<item_t>*>(pContainer);

            // Iterate over items.
            for (const auto& pGcPtr : pThis->vData) {
                onGcPtrItem(&pGcPtr);
            }
        }

        /** Actual array that stores GcPtr items. */
        std::vector<vec_item_t> vData;
    };
}
