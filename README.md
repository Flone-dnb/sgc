# How to use it

First, set garbage collector callbacks:

```Cpp
#include "GcInfoCallbacks.hpp"

static inline void gcWarningCallback(const char* pMessage) {
    // garbage collector produced a warning message
    // do something here (logging for example)
}

static inline void gcCriticalErrorCallback(const char* pMessage) {
    // garbage collector produced a critical error message
    // do something here (logging for example)
    // most likelly the GC will throw an exception or crash after this callback
}

int main() {
    sgc::GcInfoCallbacks::setCallbacks(gcWarningCallback, gcCriticalErrorCallback);
}
```

Use GC smart pointers:

```Cpp
#include "GcPtr.h"

class Foo {
public:
    Foo(int iValue) : iValue(iValue) {}
    sgc::GcPtr<Foo> pFoo; // works similar to `std::shared_ptr`

    int iValue = 0;
};

{
    const auto pFoo = sgc::makeGc<Foo>(42); // similar to `std::make_shared`
    // don't do this: `sgc::makeGc<Foo>(new Foo(42));` it will produce a critical error (see limitations section)

    pFoo->pFoo = pFoo;
}

// 1 allocation is still alive.
assert(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

// Somewhere in your code call the garbage collection to delete no longer referenced (no longer reachable) allocations.
assert(sgc::GarbageCollector::get().collectGarbage() == 1); // 1 allocation deleted

// No GC allocations exist.
assert(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
```

When you need to store a `GcPtr` in a container use special "GC containers", for example:

```Cpp
#include "GcPtr.h"
#include "gccontainers/GcVector.hpp"

// DON'T use this, this may leak memory in some cases (see limitations section).
std::vector<sgc::GcPtr<Foo>> vStdVec;

// Use this (treat as a usual `std::vector`):
sgc::GcVector<sgc::GcPtr<Foo>> vGcVec; // `GcVector` wraps `std::vector` and adds some GC related logic
```

There's no `dynamic_pointer_cast`, just use a regular `dynamic_cast`, for example:

```Cpp
sgc::GcPtr<Parent> pParent = dynamic_cast<Parent*>(pChild.get());
assert(pParent != nullptr);

// try casting to a wrong type
sgc::GcPtr<Foo> pFoo = dynamic_cast<Foo*>(pChild.get());
assert(pFoo == nullptr);
```

# Limitations

## General

Most of the cases listed below trigger a critical error message so even if you forget about something you will be notified as long as you bind to GC callbacks.

- `GcPtr` objects can only hold objects that were allocated using `makeGc` calls

```Cpp
sgc::GcPtr<Foo> pFoo1 = sgc::GcPtr<Foo>(new Foo()); // critical error
sgc::GcPtr<Foo> pFoo2 = new Foo(); // also critical error

sgc::GcPtr<Foo> pGcFoo = sgc::makeGc<Foo>();
Foo* pRawFoo = pGcFoo.get();
sgc::GcPtr<Foo> pAnotherGcFoo = pRawFoo; // perfectly valid since `Foo` object was previously allocated using `makeGc`
```

- `GcPtr` objects can fail when holding objects of types that use multiple inheritance

```Cpp
class MultiChild : public Parent1, public Parent2 { /* ... */ };

const auto pMultiChild = sgc::makeGc<MultiChild>(); // fine

// Cast to first parent.
sgc::GcPtr<Parent1> pParent1 = pMultiChild; // also fine

// Cast to second parent.
sgc::GcPtr<Parent2> pParent2 = dynamic_cast<Parent2*>(pParent1.get()); // critical error
```

- Avoid capturing `GcPtr` objects in lambdas as it may leak memory in some cases, for example:

```Cpp
class Foo {
public:
    std::function<void()> callback;
};

{
    auto pFoo = sgc::makeGc<Foo>();
    pFoo->callback = [pFoo]() {}; // create a cyclic ref

    // We have only 1 allocation.
    assert(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);
}

// 1 allocation is still alive.
assert(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

assert(sgc::GarbageCollector::get().collectGarbage() == 0); // nothing was deleted
assert(sgc::GarbageCollector::get().getAliveAllocationCount() == 1);

// Clear lambda.
pLeakedFoo->callback = []() {};

assert(sgc::GarbageCollector::get().collectGarbage() == 1);
assert(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
```

## About containers

- Storing `GcPtr` objects in "non-GC containers" such as `std::vector` generally won't leak the memory but such `GcPtr` objects will be incorrectly placed in the garbage collector's node graph and thus may cause memory leaks.

- The main problem with "non-GC containers", lambdas and similar things is cyclic reference. Imagine a class that stores some non-GC container with `GcPtr`s inside of it. Because in most cases that non-GC container stores its items in some other (allocated) place `GcPtr`s don't know that they belong to some container/object, thus the incorrect node graph makes the garbage collector think that your no longer referenced allocation is still used somewhere.

- But there are exception, such as `std::pair` or `std::array`. Storing `GcPtr` objects in `std::pair` is perfectly fine because `std::pair` does not store inner objects in some other (allocated) place while containers like `std::vector` do. Here is a very simplified example of what's going on with `std::pair`:

```Cpp
class Foo {
    std::pair<std::recursive_mutex, sgc::GcPtr<Foo>> mtxSomeFoo;
};

// This is perfectly fine because pair is just:

class pair {
    FirstType first;
    SecondType second;
}

// Thus when the `makeGc` allocates outer `Foo` object (not `mtxSomeFoo.second`), addresses (in memory) of that `first` and `second` fields will belong to the memory region of the `Foo`. This way the inner `GcPtr<Foo>` will detect that it belongs to some outer GC object.
const auto pOuterFoo = sgc::makeGc<Foo>();         // marked as a root (top-level) node
pOuterFoo->mtxSomeFoo.second = sgc::makeGc<Foo>(); // this inner `Foo` is marked as a child of the outer (root) `Foo`

const auto pOuterFoo = std::make_shared<Foo>();    // you can also do this if you want but
pOuterFoo->mtxSomeFoo.second = sgc::makeGc<Foo>(); // this inner `Foo` is marked as a root (top-level) node
                                                   // because there's no outer GC allocated `Foo`, which is valid
```

# Thread safety

- You can modify the same `GcPtr` object simultaneously from multiple threads. Note, we are talking about `GcPtr` object, not about its inner allocation that it's pointing to. For example:

```Cpp
sgc::GcPtr<Foo> pFoo = sgc::makeGc<Foo>();

// launch 2 threads and pass `pFoo` by a ref or a raw pointer (i.e. no `GcPtr` copy):

// --- thread #0 --- | ---- thread #1 ----         // you CAN do this and it should be valid since
std::move(pFoo);     |  pFoo = sgc::makeGc<Foo>(); // `GcPtr` has some synchronization inside of it but generally 
// ----------------- | -------------------         // you will just copy `GcPtr` object into another thread
```

- You can call garbage collection from a non-main thread.

- Avoid situations when no `GcPtr` object is pointing to your `makeGc` allocated object to pass it somewhere else, for example:

```Cpp
GcPtr<Foo> pGcFoo = makeGc<Foo>();
Foo* pRawFoo = pGcFoo.get();

pGcFoo = nullptr; // at this point no `GcPtr` is referencing `Foo` and thus `Foo` is unreachable (subject to be deleted)

// `Foo` can be deleted here if GC is called from another thread (not the current one)

GcPtr<Foo> pAnotherFoo = pRawFoo; // might trigger a critical error since `pRawFoo` is not `nullptr`

// -------------- instead do this: --------------

GcPtr<Foo> pGcFoo = makeGc<Foo>();
Foo* pRawFoo = pGcFoo.get();

GcPtr<Foo> pAnotherFoo = pRawFoo; // now 2 `GcPtr` objects reference `Foo`

pGcFoo = nullptr; // now just 1 `GcPtr` object references `Foo`
```

- Don't synchronously wait for another thread that operates with GC entities in the constructor/destructor of your GC-controlled type (talking only about constructor and destructor, other functions are fine), for example:

```Cpp
class Foo {
public:
    Foo() {
        const auto pPromise = std::make_shared<std::promise<bool>>();
        auto future = pPromise->get_future();

        addThread([pPromise]() {
            const auto pOther = sgc::makeGc<...>(); // or any other GC operation (will cause a deadlock here)

            pPromise->set_value(true);
        });

        future.get(); // this will cause a deadlock because we're in constructor of GC-controlled type
    }

    ~Foo() {
        // same thing here, copy-paste code from constructor and a deadlock will occur here
    }
};

const auto pFoo = sgc::makeGc<Foo>();
```

# Examples of storing GC pointers in various places

Here are some typical `GcPtr` use-cases where you might ask "is this OK?" and the answer is "yes":

```Cpp
class Collected {};

class Inner {
public:
    sgc::GcPtr<Collected> pCollected;
};

class Outer {
public:
    Inner inner; // not wrapping into a `GcPtr`
};

{
    Outer outer;
    outer.inner.pCollected = sgc::makeGc<Collected>();

    // `pCollected` is alive
} // goes out of scope but still alive, waiting for GC

sgc::GarbageCollector::get().collectGarbage();

// `pCollected` is freed now, everything runs as expected
```

Another example:

```Cpp
void foo(const sgc::GcPtr<MyClass>&);

void MyClass::bar() {
    foo(sgc::GcPtr<MyClass>(this)); // constructing a `GcPtr` from `this` is fine ONLY
                                    // IF `this` object was previously allocated using `makeGc`
}
```

Another example:

```Cpp
class Collected {};

struct MyData {
    void allocate() { pCollected = sgc::makeGc<Collected>(); }

private:
    sgc::GcPtr<Collected> pCollected;
};

{
    std::vector<MyData> vMyData;      // intentionally not using `GcVector` because not storing `GcPtr<MyData>` items
    // sgc::GcVector<MyData> vMyData; // won't compile because `GcVector` only accepts `GcPtr` items

    constexpr size_t iDataSize = 10;
    for (size_t i = 0; i < iDataSize; i++) {
        MyData data;
        data.allocate();
        vMyData.push_back(std::move(data));
    }

    // array objects are alive here
} // array goes out of scope but still alive, waiting for GC

sgc::GarbageCollector::get().collectGarbage();

// array objects are freed now, everything runs as expected
```

You can find more examples in the directory `src/sgc_tests/src`.

# How to add it to your project

In your cmake file:

```cmake
set(SGC_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(<some path here>/sgc SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC sgc_lib)
```

# Adding support for new containers

Similarly to `GcVector` you can add support for other containers to store `GcPtr`s in them. In order to implement a new "GC container" you need to use `GcVector` implementation as a reference and make sure to follow these rules:

- `GcPtr` elements in your internal container (non-GC container that you are wrapping) should pass `false` as `bCanBeRootNode` template parameter of `GcPtr`, for example: `std::vector<sgc::GcPtr<InnerType, false>>`.
- Make sure your "GC container" has the following requirement: `!std::derived_from<ValueType, GcContainerBase>` because containers inside of containers are not supported.
- Member functions that modify the container's size or its capacity (such as `push_back`, `insert`, `reserve` and etc.) must lock the mutex `GarbageCollector::get().getGarbageCollectionMutex()` until the operation is not finished, see `GcVector::push_back` as an example.
    - Same thing for copy/move constructors and assignment operators of your container.
- Make sure to call `notifyGarbageCollectorAboutDestruction` in your GC container's destructor.
- Make sure that your internal container (non-GC container that you are wrapping) stays in a valid state after it was `move`d (for example, has a size of 0) so that the garbage collector can still iterate over it without any problems after it was `move`d.
- Implement all tests from `sgc_tests/src/containers/VectorTests.cpp` according to your container's functionality.
- Add some usage of your new container to the multi-threaded tests at `src/sgc_tests/src/MultithreadingTests.cpp` (similar to how `GcVector` is used there).


# Build

Prerequisites:

- compiler that supports C++20
- [CMake](https://cmake.org/download/)
- [Doxygen](https://doxygen.nl/download.html)
- [LLVM](https://github.com/llvm/llvm-project/releases/latest)

First, clone this repository:

```
git clone <project URL>
cd <project directory name>
git submodule update --init --recursive
```

Then, if you've never used CMake before:

Create a `build` directory next to this file, open created `build` directory and type `cmd` in Explorer's address bar. This will open up a console in which you need to type this:

```
cmake -DCMAKE_BUILD_TYPE=Debug .. // for debug mode
cmake -DCMAKE_BUILD_TYPE=Release .. // for release mode
```

This will generate project files that you will use for development.

# Update

To update this repository:

```
git pull
git submodule update --init --recursive
```

# Documentation

In order to generate the documentation you need to have [Doxygen](https://www.doxygen.nl/index.html) installed.

The documentation can be generated by executing the `doxygen` command while being in the `docs` directory. If Doxygen is installed, this will be done automatically on each build.

The generated documentation will be located at `docs/gen/html`, open the `index.html` file from this directory to see the documentation.

# References

- tgc2: https://github.com/crazybie/tgc2
