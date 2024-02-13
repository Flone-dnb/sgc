#pragma once

namespace sgc {
    /** Base class for GC pointers and GC containers. */
    class GcNode {
    public:
        virtual ~GcNode() = default;

    protected:
        GcNode() = default;

        /**
         * Sets if this GC node object belongs to some other object as a field.
         *
         * @param bIsRootNode `true` if root, `false` otherwise.
         */
        inline void setIsRootNode(bool bIsRootNode) { this->bIsRootNode = bIsRootNode; }

        /**
         * Tells if this GC node object belongs to some other object as a field.
         *
         * @return `true` if root, `false` otherwise.
         */
        inline bool isRootNode() const { return bIsRootNode; }

    private:
        /**
         * Defines if this GC node object belongs to some other object as a field.
         *
         * @remark Initialized in constructor of the derived class and never changed later.
         */
        bool bIsRootNode = false;
    };
}
