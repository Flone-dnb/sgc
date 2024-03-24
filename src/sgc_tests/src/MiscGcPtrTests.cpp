// Standard.
#include <functional>

// Custom.
#include "GarbageCollector.h"
#include "GcPtr.h"
#include "gccontainers/GcVector.hpp"

// External.
#include "catch2/benchmark/catch_benchmark.hpp"
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

TEST_CASE("passed construction arguments to make gc are passed to type constructor") {
    class Foo {
    public:
        Foo() = default;
        Foo(int iValue) : iValue(iValue) {}
        Foo(std::unique_ptr<int> pNonCopyable) : pNonCopyable(std::move(pNonCopyable)) {}

        int iValue = 0;
        std::unique_ptr<int> pNonCopyable;
    };

    {
        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.empty());
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        auto pFoo1 = sgc::makeGc<Foo>();
        REQUIRE(pFoo1->iValue == 0);

        auto pFoo2 = sgc::makeGc<Foo>(2);
        REQUIRE(pFoo2->iValue == 2);

        auto pNonCopyable = std::make_unique<int>(3);
        auto pFoo3 = sgc::makeGc<Foo>(std::move(pNonCopyable));
        REQUIRE(pFoo3->iValue == 0);
        REQUIRE(pFoo3->pNonCopyable != nullptr);
        REQUIRE(*pFoo3->pNonCopyable == 3);
    }

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 3);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("gc allocations are destroyed only while collecting garbage") {
    class Foo {};

    {
        sgc::GcPtr<Foo> pFoo = sgc::makeGc<Foo>();

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

        pFoo = nullptr; // explicitly reset

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
    }

    {
        sgc::GcPtr<Foo> pFoo = sgc::makeGc<Foo>();

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    } // pFoo is destroyed here but not the Foo object

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    // Get root nodes.
    const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(*mtxRootNodes.first);

        REQUIRE(mtxRootNodes.second->gcPtrRootNodes.empty());
        REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
    }

    {
        sgc::GcPtr<Foo> pFoo; // not initialized

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    // Check root nodes.
    {
        std::scoped_lock guard(*mtxRootNodes.first);

        REQUIRE(mtxRootNodes.second->gcPtrRootNodes.empty());
        REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
    }
}

TEST_CASE("GC solves cyclic references (ref created outside constructor") {
    class Foo {
    public:
        sgc::GcPtr<Foo> pFoo;
    };

    {
        auto pFoo = sgc::makeGc<Foo>();

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        // Create cyclic ref.
        pFoo->pFoo = pFoo;

        // Check root nodes.
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
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

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
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

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE((*mtxRootNodes.second->gcPtrRootNodes.begin())->getUserObject() == pFoo.get());
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        // Run GC to apply the changes.
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);

        // Create inner.
        pFoo->pInner = sgc::makeGc<Foo>();

        // Check root nodes.
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            // There should only be 1 root node.
            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE((*mtxRootNodes.second->gcPtrRootNodes.begin())->getUserObject() == pFoo.get());
        }

        // Clear inner.
        pFoo->pInner = nullptr;

        // Run GC (inner should be deleted).
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);

        // Check root nodes.
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            // There should only be our node (because GcPtr object still exists).
            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE((*mtxRootNodes.second->gcPtrRootNodes.begin())->getUserObject() == pFoo.get());
        }
    }

    // Scope ended and pFoo and pInner were destroyed.

    // Check root nodes.
    const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(*mtxRootNodes.first);

        // There should still be our node.
        REQUIRE(mtxRootNodes.second->gcPtrRootNodes.empty());
        REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
    }

    // Run GC to apply pending changes (Foo should be deleted).
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);

    // Check root nodes.
    {
        std::scoped_lock guard(*mtxRootNodes.first);

        // All empty.
        REQUIRE(mtxRootNodes.second->gcPtrRootNodes.empty());
        REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
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

            pCollectedFromRaw = pRaw;
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        }

        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("constructing gc pointer to this from raw pointer is valid") {
    static bool bConstructorCalled = false;
    class Foo {
    public:
        Foo() {
            pThis = this;
            bConstructorCalled = true;
        }

        sgc::GcPtr<Foo> getThis() { return this; }

        sgc::GcPtr<Foo> pThis;
    };

    {
        sgc::GcPtr<Foo> pTopFoo;

        {
            auto pFoo = sgc::makeGc<Foo>();
            REQUIRE(bConstructorCalled);

            // Get root nodes.
            const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
            {
                std::scoped_lock guard(*mtxRootNodes.first);

                REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 2);
                REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
            }

            REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);

            pTopFoo = pFoo->getThis();
        }

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

        // Check root nodes.
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("copying gc pointers does not cause leaks") {
    class Foo {};

    {
        sgc::GcPtr<Foo> pPointer2;

        {
            auto pPointer1 = sgc::makeGc<Foo>();
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

            pPointer2 = pPointer1;
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        }

        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    // same thing but explicitly reset pointer
    {
        sgc::GcPtr<Foo> pPointer2;

        auto pPointer1 = sgc::makeGc<Foo>();
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

        pPointer2 = pPointer1;
        pPointer1 = nullptr;
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

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
        sgc::GcPtr<Collected> pCollected;
    };

    // Make sure no GC object exists.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    {
        auto pOuter = sgc::makeGc<Outer>();
        pOuter->pCollected = sgc::makeGc<Collected>();
        pOuter->inner.pCollected = sgc::makeGc<Collected>();

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        // There should be 2 child nodes.
        REQUIRE(sgc::GcTypeInfo::getStaticInfo<Outer>()->getGcPtrFieldOffsets().size() == 2);

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 3);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 3);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 3);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 3);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("shared pointer outer object that stores inner object with a gc field does not cause leaks") {
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
        auto pOuter = std::make_shared<Outer>();
        pOuter->inner.pCollected = sgc::makeGc<Collected>();

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("unique pointer outer object that stores inner object with a gc field does not cause leaks") {
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
        auto pOuter = std::make_unique<Outer>();
        pOuter->inner.pCollected = sgc::makeGc<Collected>();

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE(
    "std::vector of objects that have gc fields and another std::vector for refs does not cause leaks") {
    class Collected {};

    struct MyData {
        sgc::GcPtr<Collected> pCollected;
    };

    // Make sure no GC object exists.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    constexpr size_t iDataSize = 5;

    {
        std::vector<MyData> vMyDataRef; // intentionally not using `GcVector` because not storing `GcPtr`s

        {
            std::vector<MyData> vMyDataOriginal; // intentionally not using `GcVector`

            // Make sure no GC object exists.
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

            for (size_t i = 0; i < iDataSize; i++) {
                MyData data1;
                data1.pCollected = sgc::makeGc<Collected>(); // allocate
                MyData data2;
                data2.pCollected = data1.pCollected; // create ref

                vMyDataOriginal.push_back(std::move(data1));
                vMyDataRef.push_back(std::move(data2));
            }

            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == iDataSize);
        }

        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == iDataSize);
    }

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == iDataSize);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
}

TEST_CASE("create and destroy gc pointer between gc collection") {
    // Prepare custom type.
    class Foo {
    public:
    };

    {
        auto pFoo = sgc::makeGc<Foo>();

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }
    } // pFoo destroyed here

    // don't apply pending changes yet

    // Get root nodes.
    const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(*mtxRootNodes.first);

        REQUIRE(mtxRootNodes.second->gcPtrRootNodes.empty());
        REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("call make gc in constructor of gc pointer") {
    class Bar {
    public:
        sgc::GcPtr<int> pTest;
    };

    class Foo {
    public:
        Foo() { pBar = sgc::makeGc<Bar>(); }
        sgc::GcPtr<Bar> pBar;
    };

    {
        auto pFoo = sgc::makeGc<Foo>();

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
    }

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 2);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("capture gc pointer in global lambda (without cyclic ref) does not cause leaks") {
    class Foo {
    public:
        int iValue = 0;
    };

    {
        auto pFoo = sgc::makeGc<Foo>();
        pFoo->iValue = 1;

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

        const auto callback = [pFoo]() { // new root node is created due to copy
            REQUIRE(pFoo->iValue == 1);
            pFoo->iValue = 2;
        };

        // Check root nodes.
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 2);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        callback();

        // Check root nodes.
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 2);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }
    }

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("capture gc pointer in lambda to create a cyclic reference that leaks memory") {
    class Foo {
    public:
        std::function<void()> callback;
    };

    Foo* pLeakedFoo = nullptr;

    {
        auto pFoo = sgc::makeGc<Foo>();
        pFoo->callback = [pFoo]() {}; // new root node is created due to copy

        pLeakedFoo = pFoo.get(); // save raw pointer to use later

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 2);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }
    } // pFoo is destroyed here but not Foo object

    // Get root nodes.
    const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
    {
        std::scoped_lock guard(*mtxRootNodes.first);

        REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
        REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
    }

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

    // Check root nodes.
    {
        std::scoped_lock guard(*mtxRootNodes.first);

        REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
        REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
    }

    // Clear lambda.
    pLeakedFoo->callback = []() {};

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    // Check root nodes.
    {
        std::scoped_lock guard(*mtxRootNodes.first);

        REQUIRE(mtxRootNodes.second->gcPtrRootNodes.empty());
        REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
    }
}

/// Test.
class Node {
public:
    Node() {
        // Some dummy custom constructor logic.
        if (!vChildNodes.empty()) {
            REQUIRE(!vChildNodes.empty());
        }

        sSomeText = "Hello World! Hello World! Hello World!";
    }
    ~Node() {
        // Some dummy custom destructor logic.
        if (!vChildNodes.empty()) {
            REQUIRE(!vChildNodes.empty());
        }
    }

    /// Test.
    sgc::GcPtr<Node> pParent;

    /// Test.
    sgc::GcVector<sgc::GcPtr<Node>> vChildNodes;

    /// Test.
    std::string sSomeText;
};

void createNodeTree(size_t iChildrenCount, sgc::GcPtr<Node> pNode) { // NOLINT
    if (iChildrenCount == 0) {
        return;
    }

    const auto pNewNode = sgc::makeGc<Node>();
    pNewNode->pParent = pNode;

    static size_t iRandomCounter = 0;
    iRandomCounter += 1;

    if (iRandomCounter % 3 == 0) {
        // Add some empty pointers.
        pNode->vChildNodes.push_back(sgc::GcPtr<Node>());
        pNode->vChildNodes.push_back(sgc::GcPtr<Node>());
    }

    createNodeTree(iChildrenCount - 1, pNewNode);

    if (iChildrenCount % 4 == 0) {
        // Add some empty pointers.
        pNode->vChildNodes.push_back(sgc::GcPtr<Node>());
    }

    pNode->vChildNodes.push_back(pNewNode);

    if (iChildrenCount % 3 == 0) {
        // Add some empty pointers.
        pNode->vChildNodes.push_back(sgc::GcPtr<Node>());
    }
}

TEST_CASE("benchmark garbage collection") {
    {
        const auto pRootNode = sgc::makeGc<Node>();

        for (size_t i = 0; i < 10; i++) { // NOLINT
            const auto pNewNode = sgc::makeGc<Node>();
            createNodeTree(1000, pNewNode); // NOLINT
            pRootNode->vChildNodes.push_back(pNewNode);
        }

        // Get root nodes.
        const auto mtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(*mtxRootNodes.first);

            REQUIRE(mtxRootNodes.second->gcPtrRootNodes.size() == 1);
            REQUIRE(mtxRootNodes.second->gcContainerRootNodes.empty());
        }

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 10011);

        BENCHMARK("performance on ~10k node tree - not collected") {
            REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        };
    }

    BENCHMARK("performance on ~10k+ node tree - collected all (no root nodes)") {
        sgc::GarbageCollector::get().collectGarbage();
    };
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
