#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace coser {

enum class TaskStatus { Started, Running, Completed, Failed };

struct TaskProgressEvent {
    std::string task_id;
    std::string operation;  // "ingest" or "query"
    TaskStatus status = TaskStatus::Started;
    int total = 0;
    int processed = 0;
    int succeeded = 0;
    int skipped = 0;
    int failed = 0;
    std::string current_path;
    std::string message;
};

using TaskProgressCallback = std::function<void(const TaskProgressEvent&)>;

/// Process-local event hub for UI or service modules. Callbacks are invoked
/// synchronously on the worker thread that emits the event; consumers must
/// dispatch to their own UI thread and should return quickly.
class TaskProgressHub {
public:
    static TaskProgressHub& Instance();

    uint64_t Register(TaskProgressCallback callback);
    void Unregister(uint64_t subscriptionId);
    void Notify(const TaskProgressEvent& event) const;

private:
    mutable std::mutex mutex_;
    uint64_t nextSubscriptionId_ = 1;
    std::unordered_map<uint64_t, TaskProgressCallback> callbacks_;
};

const char* ToString(TaskStatus status);

}  // namespace coser
