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

# Limitations

Most of the cases listed below trigger a critical error message so even if you forget about something you will be notified as long as you bind to GC callbacks.

- `GcPtr` objects can only hold objects that were allocated using `makeGc` calls
```Cpp
sgc::GcPtr<Foo> pFoo1 = sgc::makeGc<Foo>(new Foo()); // critical error
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

# Thread safety

- You CAN modify the same `GcPtr` object simultaneously from multiple threads. Note, we are talking about `GcPtr` object, not about its inner allocation that it's pointing to. For example:

```Cpp
sgc::GcPtr<Foo> pFoo = sgc::makeGc<Foo>();

// launch 2 threads and pass `pFoo` by a ref or a raw pointer (i.e. no `GcPtr` copy):

// --- thread #0 --- | ---- thread #1 ----         // you CAN do this and it should be valid since
std::move(pFoo);     |  pFoo = sgc::makeGc<Foo>(); // `GcPtr` has some synchronization inside of it but generally 
// ----------------- | -------------------         // you will just copy `GcPtr` object into another thread
```

- You CAN call garbage collection from a non-main thread.

- Avoid situations when no `GcPtr` object is pointing to your `makeGc` allocated object to pass it somewhere else, for example:

```Cpp
GcPtr<Foo> pGcFoo = makeGc<Foo>();
Foo* pRawFoo = pGcFoo.get();

pGcFoo = nullptr; // at this point no `GcPtr` is referencing `Foo` and thus `Foo` is unreachable (subject to be deleted)

// `Foo` can be deleted here if GC is called from another thread (not the current one)

GcPtr<Foo> pAnotherFoo = pRawFoo; // might trigger a critical error since `pRawFoo` is not `nullptr`

// -------------- instead do this:

GcPtr<Foo> pGcFoo = makeGc<Foo>();
Foo* pRawFoo = pGcFoo.get();

GcPtr<Foo> pAnotherFoo = pRawFoo; // now 2 `GcPtr` objects reference `Foo`

pGcFoo = nullptr; // now just 1 `GcPtr` object references `Foo`
```

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
