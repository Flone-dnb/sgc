// Custom.
#include "GarbageCollector.h"
#include "GcPtr.h"

// External.
#include "catch2/catch_test_macros.hpp"

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
}

TEST_CASE("dynamic cast between GC pointers") {
    class Parent1 {
    public:
        virtual ~Parent1() = default;

    private:
        int iParentValue1 = 0;
    };

    class Child1 : public Parent1 {
    public:
        virtual ~Child1() override = default;

    private:
        int iTest = 0;
    };

    class Child2 : public Parent1 {
    public:
        virtual ~Child2() override = default;

    private:
        int iTest = 0;
    };

    {
        auto pChild1 = sgc::makeGc<Child1>();

        sgc::GcPtr<Parent1> pParent = pChild1;

        // Try casting to wrong child.
        sgc::GcPtr<Child2> pChild2Casted1 = dynamic_cast<Child2*>(pParent.get());
        sgc::GcPtr<Child2> pChild2Casted2(dynamic_cast<Child2*>(pParent.get()));

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        REQUIRE(pChild2Casted1 == nullptr);
        REQUIRE(pChild2Casted2 == nullptr);

        // Try casting to the correct child.
        sgc::GcPtr<Child1> pChild1Casted1 = dynamic_cast<Child1*>(pParent.get());
        sgc::GcPtr<Child1> pChild1Casted2(dynamic_cast<Child1*>(pParent.get()));

        REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
        REQUIRE(pChild1Casted1 != nullptr);
        REQUIRE(pChild1Casted2 != nullptr);
        REQUIRE(pChild1Casted1 == pChild1);
        REQUIRE(pChild1Casted2 == pChild1);
    }

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);

    class Parent2 {
    public:
        virtual ~Parent2() = default;

    private:
        int iParentValue2 = 0;
    };

    class MultiChild : public Parent1, public Parent2 {
    public:
        virtual ~MultiChild() override = default;

    private:
        int iTest = 0;
    };

    {
        auto pMultiChild = sgc::makeGc<MultiChild>();

        // Cast to first parent.
        sgc::GcPtr<Parent1> pParent1 = pMultiChild;

        // Casting to a non-first parent would cause the allocation info to be not found.
        bool bExceptionThrown = false;
        try {
            // Cast to second parent.
            sgc::GcPtr<Parent2> pParent2 = dynamic_cast<Parent2*>(pParent1.get());
        } catch (...) {
            bExceptionThrown = true;
        }
        REQUIRE(bExceptionThrown);
    }

    REQUIRE(sgc::GarbageCollector::get().collectGarbage() == 1);
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
