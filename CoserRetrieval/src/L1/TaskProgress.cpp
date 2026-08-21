#include "TaskProgress.h"

#include <vector>

namespace coser {

TaskProgressHub& TaskProgressHub::Instance() {
    static TaskProgressHub hub;
    return hub;
}

uint64_t TaskProgressHub::Register(TaskProgressCallback callback) {
    if (!callback) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t id = nextSubscriptionId_++;
    callbacks_.emplace(id, std::move(callback));
    return id;
}

void TaskProgressHub::Unregister(uint64_t subscriptionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.erase(subscriptionId);
}

void TaskProgressHub::Notify(const TaskProgressEvent& event) const {
    std::vector<TaskProgressCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks.reserve(callbacks_.size());
        for (const auto& [_, callback] : callbacks_) callbacks.push_back(callback);
    }
    for (const auto& callback : callbacks) {
        try {
            callback(event);
        } catch (...) {
            // A UI/service subscriber must never terminate the worker task.
        }
    }
}

const char* ToString(TaskStatus status) {
    switch (status) {
        case TaskStatus::Started: return "started";
        case TaskStatus::Running: return "running";
        case TaskStatus::Completed: return "completed";
        case TaskStatus::Failed: return "failed";
    }
    return "unknown";
}

}  // namespace coser
