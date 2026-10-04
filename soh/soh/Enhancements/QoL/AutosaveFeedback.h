#pragma once

#include <atomic>
#include <memory>
#include <vector>

// The game thread owns pending presentation; each worker owns only its result state.
class AutosaveFeedback {
  public:
    void* BeginSave() {
        auto result = std::make_shared<std::atomic<int>>(Pending);
        auto workerResult = std::make_unique<std::shared_ptr<std::atomic<int>>>(result);
        pending.push_back(std::move(result));
        return workerResult.release();
    }

    static void CompleteSave(int success, void* userData) {
        std::unique_ptr<std::shared_ptr<std::atomic<int>>> result(
            static_cast<std::shared_ptr<std::atomic<int>>*>(userData));
        (*result)->store(success ? Success : Failure);
    }

    template <typename Emit> void ProcessResults(Emit emit) {
        for (auto it = pending.begin(); it != pending.end();) {
            const int result = (*it)->load();
            if (result == Pending) {
                ++it;
            } else {
                it = pending.erase(it);
                emit(result == Success);
            }
        }
    }

    void Clear() {
        pending.clear();
    }

  private:
    enum { Pending, Failure, Success };
    std::vector<std::shared_ptr<std::atomic<int>>> pending;
};
