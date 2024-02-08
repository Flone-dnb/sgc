#include "GcTypeInfo.h"

// Standard.
#include <stdexcept>

// Custom.
#include "GcAllocation.h"
#include "GcInfoCallbacks.hpp"

namespace sgc {

    GcTypeInfo::GcTypeInfo(size_t iTypeSize, GcTypeInfoInvokeDestructor pInvokeDestructor)
        : pInvokeDestructor(pInvokeDestructor), iTypeSize(iTypeSize) {}

    size_t GcTypeInfo::getTypeSize() const { return iTypeSize; }

    GcTypeInfo::GcTypeInfoInvokeDestructor GcTypeInfo::getInvokeDestructor() const {
        return pInvokeDestructor;
    }

    bool GcTypeInfo::tryRegisteringGcPtrFieldOffset(GcPtrBase* pConstructedPtr, GcAllocation* pAllocation) {
        // Don't check if offsets are initialized or not yet.

        // Get address of the owner object.
        const auto pOwner = pAllocation->getAllocatedObject();

        // Make sure the specified pointer is located in the memory region of the owner.
        const auto iPtrAddress = reinterpret_cast<uintptr_t>(pConstructedPtr);
        const auto iOwnerAddress = reinterpret_cast<uintptr_t>(pOwner);
        if (iPtrAddress < iOwnerAddress || iPtrAddress >= iOwnerAddress + static_cast<uintptr_t>(iTypeSize)) {
            return false;
        }

        // This GcPtr indeed belongs to the specified allocation.
        // Now see if offsets are already initialized.
        if (bAllGcPtrFieldOffsetsInitialized) {
            // Just return `true` to tell that this GcPtr belongs to the allocation.
            return true;
        }

        // Calculate offset.
        const auto iFullOffset = iPtrAddress - iOwnerAddress;

        // Make sure we won't go out of type limit.
        if (iFullOffset > std::numeric_limits<gcptr_field_offset_t>::max()) [[unlikely]] {
            GcInfoCallbacks::getCriticalErrorCallback()(
                "calculated sub-pointer offset exceeds limit of the type used to store offsets");
            throw std::runtime_error("critical error"); // can't continue
        }

        // Add offset.
        vGcPtrFieldOffsets.push_back(static_cast<gcptr_field_offset_t>(iFullOffset));

        // Registered.
        return true;
    }

}
