#include "ProbeRegistry.hpp"

/**
 * @brief Register a named probe function in the registry
 *
 * Stores the (name, fn) pair if capacity allows. The name must remain valid
 * for the lifetime of the program (use string literals or static storage).
 */
bool ProbeRegistry::registerProbe(const char *name, Probe fn)
{
    lock_();
    bool ok = false;
    if (count_ < PROBE_MAX)
    {
        entries_[count_++] = {name, std::move(fn)};
        ok = true;
    }
    unlock_();
    return ok;
}

/**
 * @brief Call a single probe by name and write its output into dst[name]
 *
 * If found, creates/gets dst[name] as a JsonObject and invokes the probe.
 */
bool ProbeRegistry::call(const char *name, JsonDocument &dst)
{
    lock_();
    for (size_t i = 0; i < count_; ++i)
    {
        if (strcmp(entries_[i].name, name) == 0)
        {
            auto fn = entries_[i].fn; // copy callable
            unlock_();
            JsonObject obj = dst[name].to<JsonObject>();
            fn(obj);
            return true;
        }
    }
    unlock_();
    return false;
}

/**
 * @brief Collect all registered probes into the provided JSON document
 *
 * Takes a snapshot of entries under the mutex, then invokes callables outside
 * the lock to minimize time spent in the critical section.
 */
void ProbeRegistry::collectAll(JsonDocument &doc)
{
    // Snapshot under lock
    size_t n = 0;
    Entry snap[PROBE_MAX];
    lock_();
    n = count_;
    for (size_t i = 0; i < n; ++i)
        snap[i] = entries_[i];
    unlock_();

    // Execute outside the lock
    for (size_t i = 0; i < n; ++i)
    {
        JsonObject v = doc[snap[i].name].to<JsonObject>();
        snap[i].fn(v);
    }
}

/**
 * @brief Collect all probes and return as a serialized JSON string
 */
String ProbeRegistry::collectAllAsJson()
{
    JsonDocument doc;
    collectAll(doc);
    String out;
    serializeJson(doc, out);
    return out;
}