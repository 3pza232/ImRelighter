#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "ml/onnx.h"

namespace ml {

struct InferenceJob {
    uint64_t documentId = 0;
    uint64_t generation = 0;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
    std::filesystem::path modelPath;
    int inputSize = 0;
    bool useDml = false;
    bool allowCpuFallback = false;
};

struct InferenceResult {
    uint64_t documentId = 0;
    uint64_t generation = 0;
    std::vector<float> depth;
    std::string error;
    bool cancelled = false;
};

class InferenceService {
public:
    InferenceService() = default;
    ~InferenceService();

    InferenceService(const InferenceService&) = delete;
    InferenceService& operator=(const InferenceService&) = delete;

    void start(const std::filesystem::path& runtimeDll);
    void stop();

    bool busy() const { return busy_.load(); }
    bool submit(InferenceJob job);
    void cancel();
    bool poll(InferenceResult& result);

private:
    void workerLoop();

    std::filesystem::path runtimeDll_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::optional<InferenceJob> pending_;
    std::optional<InferenceResult> result_;
    std::atomic<bool> busy_{false};
    std::atomic<bool> cancelRequested_{false};
    bool stopping_ = false;

    OnnxRuntime runtime_;
    OnnxSession session_;
    std::filesystem::path loadedModel_;
    bool loadedDml_ = false;
};

} // namespace ml