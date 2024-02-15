// Custom.
#include "GarbageCollector.h"
#include "GcVector.hpp"
#include "GcPtr.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("test basic vector functionality") {
    // Prepare a type to be used as a vector element.
    class Foo {
    public:
        Foo() = delete;
        Foo(size_t iValue) : iValue(iValue) {}

        size_t iValue = 0;
    };

    {
        // Empty constructors.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            REQUIRE(vTest.size() == 0); // NOLINT: test size
            REQUIRE(vTest.empty());
        }

        // Copy constructor.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vToCopy;
            vToCopy.push_back(sgc::makeGc<Foo>(1));
            vToCopy.push_back(sgc::makeGc<Foo>(2));

            REQUIRE(vToCopy.size() == 2);
            REQUIRE(!vToCopy.empty());
            REQUIRE(vToCopy[0]->iValue == 1);
            REQUIRE(vToCopy[1]->iValue == 2);

            sgc::GcVector<sgc::GcPtr<Foo>> vTest(vToCopy);

            REQUIRE(vTest.size() == 2);
            REQUIRE(vTest[0]->iValue == 1);
            REQUIRE(vTest[1]->iValue == 2);
            REQUIRE(!vTest.empty());
        }

        // Move constructor.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vToMove;
            vToMove.push_back(sgc::makeGc<Foo>(1));
            vToMove.push_back(sgc::makeGc<Foo>(2));

            REQUIRE(vToMove.size() == 2);
            REQUIRE(!vToMove.empty());
            REQUIRE(vToMove[0]->iValue == 1);
            REQUIRE(vToMove[1]->iValue == 2);

            sgc::GcVector<sgc::GcPtr<Foo>> vTest(std::move(vToMove));

            REQUIRE(vTest.size() == 2);
            REQUIRE(vTest[0]->iValue == 1);
            REQUIRE(vTest[1]->iValue == 2);
            REQUIRE(!vTest.empty());

            REQUIRE(vToMove.empty());
            REQUIRE(vToMove.size() == 0); // NOLINT: test size
        }

        // "Count" constructor
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest(4, sgc::makeGc<Foo>(1));

            REQUIRE(vTest.size() == 4);
            REQUIRE(!vTest.empty());
            REQUIRE(vTest[0]->iValue == 1);
            REQUIRE(vTest[1]->iValue == 1);
            REQUIRE(vTest[2]->iValue == 1);
            REQUIRE(vTest[3]->iValue == 1);
            REQUIRE(vTest[0] == vTest[3]); // they point to the same object
        }

        // Copy assignment operator.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            vTest.push_back(sgc::makeGc<Foo>(1));
            vTest.push_back(sgc::makeGc<Foo>(2));

            auto vNewTest = vTest;

            REQUIRE(vNewTest.size() == 2);
            REQUIRE(!vNewTest.empty());
            REQUIRE(vNewTest[0]->iValue == 1);
            REQUIRE(vNewTest[1]->iValue == 2);

            REQUIRE(vTest.size() == 2);
            REQUIRE(!vTest.empty());
        }

        // Move assignment operator.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            vTest.push_back(sgc::makeGc<Foo>(1));
            vTest.push_back(sgc::makeGc<Foo>(2));

            auto vNewTest = std::move(vTest);

            REQUIRE(vNewTest.size() == 2);
            REQUIRE(!vNewTest.empty());
            REQUIRE(vNewTest[0]->iValue == 1);
            REQUIRE(vNewTest[1]->iValue == 2);

            REQUIRE(vTest.empty());
        }

        // Comparison operator.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            vTest.push_back(sgc::makeGc<Foo>(1));
            vTest.push_back(sgc::makeGc<Foo>(2));

            auto vNewTest = vTest;
            vNewTest.pop_back();

            REQUIRE(vTest != vNewTest);

            vNewTest.push_back(sgc::makeGc<Foo>(2));
            REQUIRE(vTest != vNewTest); // last pointers points to different allocations

            vNewTest.back() = vTest.back();
            REQUIRE(vTest == vNewTest);
        }

        // At and access operator.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            vTest.push_back(sgc::makeGc<Foo>(1));
            vTest.push_back(sgc::makeGc<Foo>(2));
            vTest.push_back(sgc::makeGc<Foo>(3));

            REQUIRE(vTest.size() == 3);
            REQUIRE(!vTest.empty());
            REQUIRE(vTest[0]->iValue == 1);
            REQUIRE(vTest[1]->iValue == 2);
            REQUIRE(vTest[2]->iValue == 3);

            vTest.at(1)->iValue = 0;
            vTest[2]->iValue = 1;

            REQUIRE(vTest.size() == 3);
            REQUIRE(!vTest.empty());
            REQUIRE(vTest[0]->iValue == 1);
            REQUIRE(vTest[1]->iValue == 0);
            REQUIRE(vTest[2]->iValue == 1);
        }

        // Front and back.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            vTest.push_back(sgc::makeGc<Foo>(1));
            vTest.push_back(sgc::makeGc<Foo>(2));

            REQUIRE(vTest.front()->iValue == 1);
            REQUIRE(vTest.back()->iValue == 2);
        }

        // Data pointer.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            vTest.push_back(sgc::makeGc<Foo>(1));
            vTest.push_back(sgc::makeGc<Foo>(2));

            vTest.data()[1]->iValue = 1;

            REQUIRE(vTest[0]->iValue == 1);
            REQUIRE(vTest[1]->iValue == 1);
        }

        // Iterators.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            vTest.push_back(sgc::makeGc<Foo>(1));
            vTest.push_back(sgc::makeGc<Foo>(2));

            size_t iSum = 0;
            for (const auto& pGcPtr : vTest) {
                iSum += pGcPtr->iValue;
            }

            REQUIRE(iSum == 3);
            iSum = 0;

            for (auto& pGcPtr : vTest) {
                iSum += pGcPtr->iValue;
            }

            REQUIRE(iSum == 3);
            iSum = 0;

            for (auto it = vTest.begin(); it != vTest.end(); ++it) {
                iSum += (*it)->iValue;
            }

            REQUIRE(iSum == 3);

            // Remove first element while iterating.
            for (auto it = vTest.begin(); it != vTest.end();) {
                if ((*it)->iValue == 1) {
                    it = vTest.erase(it);
                    continue;
                }

                ++it;
            }
            REQUIRE(vTest.size() == 1);
            REQUIRE(vTest[0]->iValue == 2);

            // Restore first element.
            vTest.insert(vTest.begin(), sgc::makeGc<Foo>(1));

            REQUIRE(vTest.size() == 2);
            REQUIRE(vTest[0]->iValue == 1);
            REQUIRE(vTest[1]->iValue == 2);

            // Same thing but using `remove_if`.
            vTest.erase(std::remove_if(
                vTest.begin(), vTest.end(), [](const auto& pGcPtr) { return pGcPtr->iValue == 1; }));

            REQUIRE(vTest.size() == 1);
            REQUIRE(vTest[0]->iValue == 2);
        }

        // Reserve.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;

            REQUIRE(vTest.capacity() == 0);

            vTest.reserve(2);

            REQUIRE(vTest.size() == 0); // NOLINT: test size
            REQUIRE(vTest.empty());
            REQUIRE(vTest.capacity() > 0);

            vTest.push_back(sgc::makeGc<Foo>(1));
            vTest.push_back(sgc::makeGc<Foo>(2));

            REQUIRE(vTest[0]->iValue == 1);
            REQUIRE(vTest[1]->iValue == 2);
        }

        // Clear and shrink to fit.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            vTest.push_back(sgc::makeGc<Foo>(1));
            vTest.push_back(sgc::makeGc<Foo>(2));

            REQUIRE(vTest.capacity() > 0);

            vTest.clear();

            REQUIRE(vTest.capacity() > 0);

            vTest.shrink_to_fit();

            REQUIRE(vTest.capacity() == 0);
        }

        // Emplace back.
        {
            auto pGcPtr = sgc::makeGc<Foo>(2);

            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            vTest.emplace_back(pGcPtr);
            vTest.emplace_back(pGcPtr);

            REQUIRE(vTest.size() == 2);
            REQUIRE(vTest[0]->iValue == 2);
            REQUIRE(vTest[1]->iValue == 2);
        }

        // Resize.
        {
            sgc::GcVector<sgc::GcPtr<Foo>> vTest;
            vTest.push_back(sgc::makeGc<Foo>(1));
            vTest.push_back(sgc::makeGc<Foo>(2));

            vTest.resize(3);

            REQUIRE(vTest.size() == 3);
            REQUIRE(vTest[0]->iValue == 1);
            REQUIRE(vTest[1]->iValue == 2);
            REQUIRE(vTest[2] == nullptr);
        }
    }

    sgc::GarbageCollector::get().collectGarbage();
}

TEST_CASE("new vector elements are not registered as root nodes") {
    {
        // Create container.
        sgc::GcVector<sgc::GcPtr<int>> vTest;

        {
            // Create some pointer.
            sgc::GcPtr<int> pValue = sgc::makeGc<int>(1);

            // Insert.
            vTest.push_back(sgc::GcPtr<int>()); // insert empty pointer
            vTest.push_back(pValue);            // insert non-empty pointer

            // Get pending changes.
            const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
            {
                std::scoped_lock guard(pMtxPendingChanges->first);

                REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.size() == 1);
                REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

                REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.size() == 1);
                REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
            }

            // Get root nodes.
            const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
            {
                std::scoped_lock guard(pMtxRootNodes->first);

                REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
                REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
            }

            // Run GC to apply the changes.
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
            REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
            REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

            // Check pending changes.
            {
                std::scoped_lock guard(pMtxPendingChanges->first);

                REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
                REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

                REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
                REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
            }

            // Check root nodes.
            {
                std::scoped_lock guard(pMtxRootNodes->first);

                REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.size() == 1);
                REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
            }
        } // pValue GC pointer is destroyed here

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.size() == 1);
        }

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.size() == 1);
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
        }

        // Object should still be alive (since there is a pointer in our vector).
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.size() == 1);
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
        }
    }

    // Can now be deleted.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("vector is non root node when used as a field in GC object") {
    {
        class Foo {
        public:
            // Create container.
            sgc::GcVector<sgc::GcPtr<int>> vTest;
        };

        auto pFoo = sgc::makeGc<Foo>();

        // Insert.
        pFoo->vTest.push_back(sgc::GcPtr<int>());   // insert empty pointer
        pFoo->vTest.push_back(sgc::makeGc<int>(1)); // insert non-empty pointer

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.size() == 1);
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
        }

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }

        // Run GC to apply the changes.
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.size() == 1);
        }

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);
    }

    // Can now be deleted.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 2);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 2);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("vector is root node when used as a field in non-GC object") {
    {
        class Foo {
        public:
            // Create container.
            sgc::GcVector<sgc::GcPtr<int>> vTest;
        };

        Foo foo;

        // Insert.
        foo.vTest.push_back(sgc::GcPtr<int>());   // insert empty pointer
        foo.vTest.push_back(sgc::makeGc<int>(1)); // insert non-empty pointer

        // Get pending changes.
        const auto pMtxPendingChanges = sgc::GarbageCollector::get().getPendingNodeGraphChanges();
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.size() == 1);
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
        }

        // Get root nodes.
        const auto pMtxRootNodes = sgc::GarbageCollector::get().getRootNodes();
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.empty());
        }

        // Run GC to apply the changes.
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

        // Check pending changes.
        {
            std::scoped_lock guard(pMtxPendingChanges->first);

            REQUIRE(pMtxPendingChanges->second.newGcContainerRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcContainerRootNodes.empty());

            REQUIRE(pMtxPendingChanges->second.newGcPtrRootNodes.empty());
            REQUIRE(pMtxPendingChanges->second.destroyedGcPtrRootNodes.empty());
        }

        // Check root nodes.
        {
            std::scoped_lock guard(pMtxRootNodes->first);

            REQUIRE(pMtxRootNodes->second.gcContainerRootNodes.size() == 1);
            REQUIRE(pMtxRootNodes->second.gcPtrRootNodes.empty());
        }

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 0);
        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    }

    // Can now be deleted.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
