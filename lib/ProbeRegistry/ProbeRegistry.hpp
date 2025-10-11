#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

// ====== Tuning ======
/**
 * @def PROBE_MAX
 * @brief Maximum number of probes that can be registered in the registry
 *
 * Controls the fixed capacity of the ProbeRegistry. Each probe is a named
 * callable that writes JSON data into a document. Increase if your application
 * needs to expose more metrics or status blocks.
 */
#ifndef PROBE_MAX
#define PROBE_MAX 16 // max number of registered probes
#endif

/**
 * @brief Lightweight registry for on-demand JSON probes (metrics/status)
 *
 * The ProbeRegistry lets modules register "probes" (functions) that describe
 * their current state into a JSON document. At any time, callers can collect
 * a subset or all probes to build a structured status payload.
 *
 * Design goals:
 * - Zero dynamic allocations inside the registry (fixed capacity)
 * - Thread-safety on ESP32 via a mutex (no-ops on other targets)
 * - Simple API for registration and collection
 *
 * Usage:
 * - Register during setup(): ProbeRegistry::instance().registerProbe("wifi", fn)
 * - Collect later: JsonDocument doc; ProbeRegistry::instance().collectAll(doc);
 */
class ProbeRegistry
{
public:
    using Probe = std::function<void(JsonObject &)>;

    /**
     * @brief Singleton accessor for the registry instance
     *
     * Uses Meyers' singleton pattern to provide a single global registry
     * without dynamic allocation.
     */
    static ProbeRegistry &instance()
    {
        static ProbeRegistry inst; // Meyers singleton
        return inst;
    }

    /**
     * @brief Register a named probe function
     *
     * Registers a callable that writes its state into the provided JSON object.
     * The name becomes the key under which the probe's JSON object is stored.
     *
     * Constraints:
     * - Register during setup() or initialization
     * - The name pointer must remain valid for program lifetime (use string literal)
     * - Fails when capacity PROBE_MAX is reached
     *
     * @param name Key name for the probe (must outlive the program)
     * @param fn Callable taking a JsonObject& to populate with probe data
     * @retval true Registration succeeded
     * @retval false Registry full or invalid input
     */
    bool registerProbe(const char *name, Probe fn);

    /**
     * @brief Invoke a single probe by name and write to a destination document
     *
     * Creates/gets doc[name] as a JSON object and invokes the probe to populate it.
     *
     * @param name Probe name to call
     * @param dst Destination JSON document to write into
     * @retval true Probe found and executed
     * @retval false Probe not found
     */
    bool call(const char *name, JsonDocument &dst);

    /**
     * @brief Collect all registered probes into the provided JSON document
     *
     * Takes a snapshot of registered probes under lock, then executes them
     * outside of the lock to avoid long critical sections.
     *
     * @param doc Destination JSON document (caller must provide capacity)
     */
    void collectAll(JsonDocument &doc);

    /**
     * @brief Convenience helper to collect all probes as a JSON string
     *
     * @return String Serialized JSON payload containing all probe data
     */
    String collectAllAsJson();

    // Optional: selective collect (filter by predicate)
    template <typename Pred>
    void collectWhere(JsonDocument &doc, Pred pred)
    {
        Entry snap[PROBE_MAX];
        size_t n = 0;

        lock_();
        for (size_t i = 0; i < count_; ++i)
        {
            if (pred(entries_[i].name))
                snap[n++] = entries_[i];
        }
        unlock_();

        for (size_t i = 0; i < n; ++i)
        {
            snap[i].fn(doc[snap[i].name]);
        }
    }

private:
    /**
     * @brief Internal registry entry for a probe
     */
    struct Entry
    {
        const char *name; ///< Key name for the probe (must outlive program)
        Probe fn;         ///< Callable that fills JsonObject with current state
    };

    Entry entries_[PROBE_MAX];
    size_t count_ = 0;

    // ---------- (future) thread-safety ----------
#if CONFIG_FREERTOS_UNICORE || defined(ARDUINO_ARCH_ESP32)
    /** Acquire registry mutex (ESP32 only; no-op elsewhere) */
    void lock_()
    {
        if (mtx_)
            xSemaphoreTake(mtx_, portMAX_DELAY);
    }
    /** Release registry mutex (ESP32 only; no-op elsewhere) */
    void unlock_()
    {
        if (mtx_)
            xSemaphoreGive(mtx_);
    }
    /** Construct registry and create mutex (ESP32 only) */
    ProbeRegistry()
    {
        mtx_ = xSemaphoreCreateMutex(); // created on first use of instance()
    }
    /** Never destroyed; process lifetime matches program lifetime */
    ~ProbeRegistry() { /* never destroyed */ }
    SemaphoreHandle_t mtx_ = nullptr;
#else
    void lock_() {}
    void unlock_() {}
    ProbeRegistry() = default;
    ~ProbeRegistry() = default;
#endif

    ProbeRegistry(const ProbeRegistry &) = delete;
    ProbeRegistry &operator=(const ProbeRegistry &) = delete;
};
