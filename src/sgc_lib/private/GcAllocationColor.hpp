#pragma once

namespace sgc {
    /** Represents object's color in triple-color mark and sweep algorithm (ignoring gray color). */
    enum class GcAllocationColor : unsigned char { WHITE, BLACK };
}
