#pragma once

#if defined(DEBUG_LOGGING_ENABLED)

#if !defined(DEBUG)
static_assert(false, "expected debug build");
#endif

// Standard.
#include <memory>
#include <string>
#include <filesystem>
#include <fstream>

// External.
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace sgc {
    class ScopedDebugLog;

    /** Logs information used in debugging. */
    class DebugLogger {
        // Enables or disables logging.
        friend class ScopedDebugLog;

    public:
        DebugLogger(const DebugLogger&) = delete;
        DebugLogger& operator=(const DebugLogger&) = delete;

        ~DebugLogger() {
            // Make sure the log is flushed.
            pSpdLogger->flush();

            // Explicitly destroy spdlogger here, because logging in destructor of gc stored class can crash.
            pSpdLogger = nullptr;
        }

        /**
         * Returns a reference to the logger instance.
         * If no instance was created yet, this function will create it
         * and return a reference to it.
         *
         * @return Reference to the logger instance.
         */
        static inline DebugLogger& get() {
            static DebugLogger logger;
            return logger;
        }

        /**
         * Logs a message and flushes log to disk.
         *
         * @param sText Text to write to log.
         */
        inline void logAndFlush(std::string_view sText) const {
            if (pSpdLogger == nullptr || !bEnableLogging) {
                return;
            }

            // Get thread ID to add to log.
            std::stringstream currentThreadIdString;
            currentThreadIdString << std::this_thread::get_id();

            pSpdLogger->info(std::format("[thread {}] {}", currentThreadIdString.str(), sText));
            pSpdLogger->flush();
        }

    private:
        DebugLogger() {
            // Prepare log directory.
            auto sLoggerFilePath = std::filesystem::temp_directory_path() / sLogDirectory;

            std::filesystem::remove_all(sLoggerFilePath);
            std::filesystem::create_directories(sLoggerFilePath);

            sLoggerFilePath /= getDateTime() + sLogFileExtension;

            if (!std::filesystem::exists(sLoggerFilePath)) {
                // Create file.
                std::ofstream logFile(sLoggerFilePath);
                logFile.close();
            }

            // Prepare spdlog.
            auto fileSink =
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(sLoggerFilePath.string(), true);
            pSpdLogger = std::unique_ptr<spdlog::logger>(new spdlog::logger("MainLogger", fileSink));
            pSpdLogger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
        }

        /**
         * Returns current date and time in format "month.day_hour-minute-second".
         *
         * @return Date and time string.
         */
        static inline std::string getDateTime() {
            const time_t now = time(nullptr);

            tm tm{}; // NOLINT
#if defined(WIN32)
            const auto iError = localtime_s(&tm, &now);
            if (iError != 0) {
                throw std::runtime_error(std::format("failed to get localtime (error code {})", iError));
            }
#elif __linux__
            localtime_r(&now, &tm);
#else
            static_assert(false, "not implemented");
#endif

            return std::format("{}.{}_{}-{}-{}", 1 + tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        }

        /** Spdlog logger. */
        std::unique_ptr<spdlog::logger> pSpdLogger = nullptr;

        /** Modified by scoped log objects. */
        bool bEnableLogging = false;

        /** Name of the directory that stores logs. */
        inline static const char* sLogDirectory = "small_garbage_collector_debug_logs";

        /** Extension of the log files. */
        inline static const char* sLogFileExtension = ".log";
    };

    /** RAII-style type that enables logging on construction and disables it on destruction. */
    class ScopedDebugLog {
    public:
        ScopedDebugLog() {
            DebugLogger::get().bEnableLogging = true;
            DebugLogger::get().logAndFlush("logging enabled");
        };

        ~ScopedDebugLog() {
            DebugLogger::get().logAndFlush("logging disabled");
            DebugLogger::get().bEnableLogging = false;
        }

        ScopedDebugLog(const ScopedDebugLog&) = delete;
        ScopedDebugLog& operator=(const ScopedDebugLog&) = delete;
    };
}

#define SGC_DEBUG_LOG(text) sgc::DebugLogger::get().logAndFlush(text);
#define SGC_DEBUG_LOG_SCOPE sgc::ScopedDebugLog log;
#else
#define SGC_DEBUG_LOG(text)
#define SGC_DEBUG_LOG_SCOPE
#endif
