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
