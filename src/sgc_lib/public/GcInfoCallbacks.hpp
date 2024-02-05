#pragma once

namespace sgc {
    /** Callback which is triggered when a garbage collector produces a warning. */
    typedef void (*GcWarningCallback)(const char* pMessage);

    /**
     * Callback which is triggered when a garbage collector hits a critical error and cannot continue
     * the execution.
     */
    typedef void (*GcCriticalErrorCallback)(const char* pMessage);

    /** Provides static functions for GC warning/error callbacks. */
    class GcInfoCallbacks {
    public:
        GcInfoCallbacks() = delete;

        /**
         * Sets custom callbacks.
         *
         * @param pWarningCallback       Callback triggered by garbage collector when a garbage collector
         * warning is produced.
         * @param pCriticalErrorCallback Callback triggered by garbage collector when a critical garbage
         * collector error is produced.
         */
        static inline void
        setCallbacks(GcWarningCallback pWarningCallback, GcCriticalErrorCallback pCriticalErrorCallback) {
            pGcWarningCallback = pWarningCallback;
            pGcCriticalErrorCallback = pCriticalErrorCallback;
        }

        /**
         * Returns callback to produce warnings.
         *
         * @return Function pointer.
         */
        static inline GcWarningCallback getWarningCallback() { return pGcWarningCallback; }

        /**
         * Returns callback to produce critical errors.
         *
         * @return Function pointer.
         */
        static inline GcCriticalErrorCallback getCriticalErrorCallback() { return pGcCriticalErrorCallback; }

    private:
        /**
         * Default GC warning callback.
         *
         * @param pMessage Warning message.
         */
        static inline void defaultGcWarningCallback(const char* pMessage) {
            // Does nothing.
        }

        /**
         * Default GC critical error callback.
         *
         * @param pMessage Error message.
         */
        static inline void defaultGcCriticalErrorCallback(const char* pMessage) {
            // Does nothing.
        }

        /** Callback triggered by garbage collector when a garbage collector warning is produced. */
        static inline GcWarningCallback pGcWarningCallback = defaultGcWarningCallback;

        /** Callback triggered by garbage collector when a critical garbage collector error is produced. */
        static inline GcCriticalErrorCallback pGcCriticalErrorCallback = defaultGcCriticalErrorCallback;
    };
}
