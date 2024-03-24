// Standard.
#include <functional>
#include <format>
#include <iostream>
#include <future>

// Custom.
#include "GarbageCollector.h"
#include "GcPtr.h"
#include "gccontainers/GcVector.hpp"
#include "ThreadPool.h"
#include "DebugLogger.hpp"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("allocate GC objects and collect garbage from multiple threads") {
    class Foo {
    public:
        Foo() = delete;

        Foo(size_t iChildCount) : iChildCount(iChildCount) {
            sTest = "Hello world!";

            SGC_DEBUG_LOG(std::format("Foo object with child count {} is being created", iChildCount));

            if (iChildCount == 0) {
                return;
            }
            pInnerFoo = sgc::makeGc<Foo>(iChildCount - 1);
        }

        ~Foo() {
            SGC_DEBUG_LOG(std::format("Foo object with child count {} is being destroyed", iChildCount));
        }

        std::string sTest;
        sgc::GcPtr<Foo> pInnerFoo;

        const size_t iChildCount;
    };

    ThreadPool threadPool;

    SGC_DEBUG_LOG_SCOPE;

    {
        std::atomic<size_t> iAdditionTasksInProgress{0};
        std::atomic<size_t> iTotalObjectsCollected{0};

        const auto iIterationCount = 50; // NOLINT: test a lot of iterations to be extra sure

        const auto iThreadPoolThreadCount = threadPool.getThreadCount();
        REQUIRE(iThreadPoolThreadCount >= 2);
        const auto iAdditionTaskCountBeforeGc = iThreadPoolThreadCount - 1;
        const auto iWaitForThreadIntervalInSec = 1;
        std::atomic_flag cancelTask;

        for (size_t i = 0; i < iIterationCount; i++) { // NOLINT
            // Log status.
            auto sMessage =
                std::format("multi-threaded test, iteration: {}/{} started", i + 1, iIterationCount);
            SGC_DEBUG_LOG(sMessage);
            std::cout << sMessage << std::endl; // NOLINT: flush stream

            // Add worker tasks.
            std::atomic_flag taskStarted;
            for (size_t k = 0; k < iAdditionTaskCountBeforeGc; k++) { // NOLINT
                threadPool.addTask([&taskStarted, &cancelTask, &iAdditionTasksInProgress]() {
                    iAdditionTasksInProgress.fetch_add(1);

                    sgc::GcVector<sgc::GcPtr<Foo>> vSomeFoos;

                    taskStarted.test_and_set();

                    while (!cancelTask.test()) {
                        SGC_DEBUG_LOG("task iteration started");

                        auto pFoo = sgc::makeGc<Foo>(50); // NOLINT

                        SGC_DEBUG_LOG(std::format(
                            "task is adding created GcPtr {} to container(s)",
                            reinterpret_cast<uintptr_t>(&pFoo)));

                        vSomeFoos.push_back(pFoo); // NOLINT

                        SGC_DEBUG_LOG("task iteration finished");
                    }
                    vSomeFoos.clear();

                    iAdditionTasksInProgress.fetch_sub(1);
                });
            }

            // Wait for at least one task to start.
            while (!taskStarted.test()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // NOLINT
            }

            // Run GC task.
            const auto pGcThreadFinished = std::make_shared<std::promise<bool>>();
            auto gcThreadFuture = pGcThreadFinished->get_future();
            threadPool.addTask([pGcThreadFinished, &iTotalObjectsCollected, &iAdditionTasksInProgress]() {
                const auto iTaskCount = iAdditionTasksInProgress.load();
                if (iTaskCount == 0) {
                    SGC_DEBUG_LOG("error: no tasks are running while collecting garbage");
                    REQUIRE(false);
                }

                SGC_DEBUG_LOG(std::format("GC is running while there are {} tasks running", iTaskCount));

                iTotalObjectsCollected.fetch_add(sgc::GarbageCollector::get().collectGarbage());
                pGcThreadFinished->set_value(true);
            });

            // Wait for GC thread to finish.
            gcThreadFuture.get();

            // Stop some tasks if needed.
            size_t iTotalSecondsInSleep = 0;
            while (iAdditionTasksInProgress.load() != 0) {
                // Wait for some tasks to catch up.
                SGC_DEBUG_LOG(
                    std::format("waiting for some threads to finish ({} sec)...", iTotalSecondsInSleep));
                cancelTask.test_and_set();

                std::this_thread::sleep_for(std::chrono::seconds(iWaitForThreadIntervalInSec));
                iTotalSecondsInSleep += iWaitForThreadIntervalInSec;

                REQUIRE(iTotalSecondsInSleep < 30); // NOLINT
            }
            cancelTask.clear();

            // Log status.
            sMessage = std::format("multi-threaded test, iteration: {}/{} finished", i + 1, iIterationCount);
            SGC_DEBUG_LOG(sMessage);
            std::cout << sMessage << std::endl; // NOLINT: flush stream
        }

        // Stop thread pool.
        threadPool.stop();

        REQUIRE(iTotalObjectsCollected.load() > 0);
        REQUIRE(iAdditionTasksInProgress.load() == 0);
    }

    // Cleanup.
    sgc::GarbageCollector::get().collectGarbage();
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
