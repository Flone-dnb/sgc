// Custom.
#include "GarbageCollector.h"
#include "GcPtr.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("gc pointer comparison") {
    class Foo {};

    {
        const sgc::GcPtr<Foo> pUninitialized;
        const auto pFoo = sgc::makeGc<Foo>();

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

        REQUIRE(pUninitialized == nullptr);
        REQUIRE(pUninitialized != pFoo);
        REQUIRE(!pUninitialized); // implicit conversion to bool
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("GC solves cyclic references (ref created outside constructor") {
    class Foo {
    public:
        sgc::GcPtr<Foo> pFoo;
    };

    {
        auto pFoo = sgc::makeGc<Foo>();

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.size() == 1);
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        // Create cyclic ref.
        pFoo->pFoo = pFoo;

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.size() == 1);
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("GC solves cyclic references (ref created inside constructor") {
    class Foo {
    public:
        Foo() { pFoo = this; }
        sgc::GcPtr<Foo> pFoo;
    };

    {
        auto pFoo = sgc::makeGc<Foo>();
        REQUIRE(pFoo->pFoo != nullptr);

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.size() == 1);
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("use makeGc to create a GcPtr root node") {
    // Prepare custom type.
    class Foo {
    public:
        sgc::GcPtr<Foo> pInner;
    };

    {
        // Create root node.
        auto pFoo = sgc::makeGc<Foo>();

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            // There should only be 1 new root node.
            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.size() == 1);
            REQUIRE((*pMtxPendingChanges->second.newGcPtrRootNodes.begin())->getUserObject() == pFoo.get());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // There should be no root nodes yet because pending changes were not applied yet.
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }

        // Run GC to apply the changes.
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);

        // Create inner.
        pFoo->pInner = sgc::makeGc<Foo>();

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            // Temporary root node that was created in `makeGc` was created and destroyed.
            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // There should only be 1 root node.
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
            REQUIRE((*pMtxRootNodes->second.gcPtrRootNodes.begin())->getUserObject() == pFoo.get());
        }

        // Clear inner.
        pFoo->pInner = nullptr;

        // Run GC (inner should be deleted).
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            // There should be no nodes.
            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            // There should only be our node (because GcPtr object still exists).
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
            REQUIRE((*pMtxRootNodes->second.gcPtrRootNodes.begin())->getUserObject() == pFoo.get());
        }
    }

    // Scope ended and pFoo and pInner were destroyed.

    // Check pending changes.
    const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        // There should only be our destroyed root node.
        REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.size() == 1);
        REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());

        REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
    }

    // Check root nodes.
    const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(pMtxRootNodes->first);

        // There should still be our node.
        REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
        REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
    }

    // Run GC to apply pending changes (Foo should be deleted).
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);

    // Check pending changes.
    {
        std::scoped_lock guard(pMtxPendingChanges->first);

        // All empty.
        REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());

        REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
        REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
    }

    // Check root nodes.
    {
        std::scoped_lock guard(pMtxRootNodes->first);

        // All empty.
        REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
        REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
    }
}

TEST_CASE("constructing gc pointer from raw pointer is valid") {
    class Foo {};

    {
        sgc::GcPtr<Foo> pCollectedFromRaw = nullptr;

        {
            const auto pCollected = sgc::makeGc<Foo>();
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

            Foo* pRaw = pCollected.get();
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

            pCollectedFromRaw = sgc::GcPtr<Foo>(pRaw);
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        }

        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("moving gc pointers does not cause leaks") {
    class Foo {};

    {
        sgc::GcPtr<Foo> pPointer2;

        {
            auto pPointer1 = sgc::makeGc<Foo>();
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

            pPointer2 = std::move(pPointer1);
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        }

        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("non gc pointer outer object that stores inner object with a gc field does not cause leaks") {
    class Collected {};

    class Inner {
    public:
        sgc::GcPtr<Collected> pCollected;
    };

    class Outer {
    public:
        Inner inner;
    };

    // Make sure no GC object exists.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    {
        Outer outer;
        outer.inner.pCollected = sgc::makeGc<Collected>();

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("gc pointer outer object that stores inner object with a gc field does not cause leaks") {
    class Collected {};

    class Inner {
    public:
        sgc::GcPtr<Collected> pCollected;
    };

    class Outer {
    public:
        Inner inner;
    };

    // Make sure no GC object exists.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    {
        auto pOuter = sgc::makeGc<Outer>();
        pOuter->inner.pCollected = sgc::makeGc<Collected>();

        // Check pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.size() == 1);

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());
        }

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 2);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
