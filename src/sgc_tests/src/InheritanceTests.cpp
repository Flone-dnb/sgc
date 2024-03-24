// Custom.
#include "GarbageCollector.h"
#include "GcPtr.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("object of child type in pointer of parent type counts as root node") {
    class Parent {
    public:
        virtual ~Parent() {}
    };

    class Child : public Parent {
    public:
        virtual ~Child() override {}
    };

    {
        sgc::GcPtr<Child> pChild = sgc::makeGc<Child>();
        sgc::GcPtr<Parent> pParent = pChild;

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.size() == 2);
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }

        // Apply changes.
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 2);
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }
    }

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    // Get pending changes.
    const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

        REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
    }

    // Get root nodes.
    const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(pMtxRootNodes->first);

        REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
        REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
    }
}

TEST_CASE("object of child type in pointer of parent type using operator=") {
    static bool bParentConstructed = false;
    static bool bParentDestructed = false;
    static bool bChildConstructed = false;
    static bool bChildDestructed = false;

    class Parent {
    public:
        Parent() {
            REQUIRE(!bChildConstructed);
            bParentConstructed = true;
        }
        virtual ~Parent() {
            REQUIRE(bChildDestructed);
            bParentDestructed = true;
        }
    };

    class Child : public Parent {
    public:
        Child() {
            REQUIRE(bParentConstructed);
            bChildConstructed = true;
        }
        virtual ~Child() override {
            REQUIRE(!bParentDestructed);
            bChildDestructed = true;
        }
    };

    class Foo {
    public:
    };

    {
        // Create root node.
        sgc::GcPtr<Parent> pChild = sgc::makeGc<Child>();

        REQUIRE(bParentConstructed);
        REQUIRE(bChildConstructed);
    }

    REQUIRE(!bParentDestructed);
    REQUIRE(!bChildDestructed);

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);

    REQUIRE(bParentDestructed);
    REQUIRE(bChildDestructed);

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    // Get pending changes.
    const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

        REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
    }

    // Get root nodes.
    const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(pMtxRootNodes->first);

        REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
        REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
    }
}

TEST_CASE("object of child type in pointer of parent type using constructor") {
    static bool bParentConstructed = false;
    static bool bParentDestructed = false;
    static bool bChildConstructed = false;
    static bool bChildDestructed = false;

    class Parent {
    public:
        Parent() {
            REQUIRE(!bChildConstructed);
            bParentConstructed = true;
        }
        virtual ~Parent() {
            REQUIRE(bChildDestructed);
            bParentDestructed = true;
        }
    };

    class Child : public Parent {
    public:
        Child() {
            REQUIRE(bParentConstructed);
            bChildConstructed = true;
        }
        virtual ~Child() override {
            REQUIRE(!bParentDestructed);
            bChildDestructed = true;
        }
    };

    {
        // Create root node.
        sgc::GcPtr<Parent> pChild(sgc::makeGc<Child>());

        REQUIRE(bParentConstructed);
        REQUIRE(bChildConstructed);
    }

    REQUIRE(!bParentDestructed);
    REQUIRE(!bChildDestructed);

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);

    REQUIRE(bParentDestructed);
    REQUIRE(bChildDestructed);

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    // Get pending changes.
    const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

        REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
    }

    // Get root nodes.
    const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(pMtxRootNodes->first);

        REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
        REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
    }
}

TEST_CASE("sub gc ptr field offsets are correct") {
    class Parent {
    public:
        virtual ~Parent() = default;

        sgc::GcPtr<int> pParentPtr;
    };

    class Child : public Parent {
    public:
        virtual ~Child() override = default;

        sgc::GcPtr<int> pChildPtr;
    };

    {
        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }
    }

    {
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

        sgc::GcPtr<Parent> pParent = sgc::makeGc<Parent>();
        sgc::GcPtr<Child> pChild = sgc::makeGc<Child>();

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);

        const auto pParentTypeInfo = sgc::GcTypeInfo::getStaticInfo<Parent>();
        const auto pChildTypeInfo = sgc::GcTypeInfo::getStaticInfo<Child>();

        REQUIRE(pParentTypeInfo->getGcPtrFieldOffsets().size() == 1);
        REQUIRE(pChildTypeInfo->getGcPtrFieldOffsets().size() == 2);
    }

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 2);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
