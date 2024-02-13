#pragma once

// Standard.
#include <functional>

// Custom.
#include "GcNode.hpp"

namespace sgc {
    class GcPtrBase;

    /** Base class for containers that store `GcPtr` items. */
    class GcContainerBase : public GcNode {
    public:
        /** Signature of the function to iterate over container's GcPtr items. */
        using IterateOverContainerGcPtrItems = void (*)(
            const GcContainerBase* pContainer, const std::function<void(const GcPtrBase*)>& onGcPtrItem);

        GcContainerBase() = delete;

        virtual ~GcContainerBase() override = default;

        /**
         * Returns pointer to a function to iterate over container's GcPtr items.
         *
         * @return Pointer to a static function.
         */
        IterateOverContainerGcPtrItems getFunctionToIterateOverGcPtrItems() const;

    protected:
        /**
         * Pointer to a static function of a derived class to iterate over container's GcPtr items.
         *
         * @param pIterateOverContainerGcPtrItems Pointer to a static function.
         */
        GcContainerBase(IterateOverContainerGcPtrItems pIterateOverContainerGcPtrItems);

        /**
         * Must be called by derived classes in their destructor to notify the GC about container
         * destruction so that GC would know that it can no longer iterate over the container.
         */
        void notifyGarbageCollectorAboutDestruction();

    private:
        /** Pointer to a static function of a derived class to iterate over container's GcPtr items. */
        IterateOverContainerGcPtrItems const pIterateOverContainerGcPtrItems = nullptr;
    };
}
