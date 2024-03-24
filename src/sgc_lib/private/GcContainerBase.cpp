#include "GcContainerBase.h"

// Custom.
#include "GarbageCollector.h"

namespace sgc {

    GcContainerBase::GcContainerBase(IterateOverContainerGcPtrItems pIterateOverContainerGcPtrItems)
        : pIterateOverContainerGcPtrItems(pIterateOverContainerGcPtrItems) {
        // Notify garbage collector.
        setIsRootNode(GarbageCollector::get().onGcNodeConstructed(this));
    }

    void GcContainerBase::notifyGarbageCollectorAboutDestruction() {
        // Make sure the GC has finished iterating over the container.
        std::scoped_lock guard(*GarbageCollector::get().getGcNodeGraphMutex());

        if (isRootNode()) {
            // Notify garbage collector.
            GarbageCollector::get().onGcRootNodeBeingDestroyed(this);
        }
    }

    GcContainerBase::IterateOverContainerGcPtrItems
    GcContainerBase::getFunctionToIterateOverGcPtrItems() const {
        return pIterateOverContainerGcPtrItems;
    }

}
