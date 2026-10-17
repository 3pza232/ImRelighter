#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct OrtApi;
struct OrtEnv;
struct OrtSession;
struct OrtMemoryInfo;
struct OrtRunOptions;
struct OrtSessionOptions;
struct OrtStatus;

namespace ml {

class OnnxRuntime {
public:
    OnnxRuntime() = default;
    ~OnnxRuntime();

    OnnxRuntime(const OnnxRuntime&) = delete;
    OnnxRuntime& operator=(const OnnxRuntime&) = delete;

    bool load(const std::filesystem::path& dllPath, std::string* error);
    const OrtApi* api() const { return api_; }
    bool hasDml() const { return dmlAppend_ != nullptr; }
    OrtStatus* appendDml(OrtSessionOptions* options) const;

private:
    using DmlAppendFn = OrtStatus* (*)(OrtSessionOptions*, int);
    void* module_ = nullptr;
    const OrtApi* api_ = nullptr;
    DmlAppendFn dmlAppend_ = nullptr;
};

struct ModelInfo {
    std::string inputType;
    std::vector<int64_t> inputShape;
    std::string outputType;
    std::vector<int64_t> outputShape;
};

class OnnxSession {
public:
    OnnxSession() = default;
    ~OnnxSession();

    OnnxSession(const OnnxSession&) = delete;
    OnnxSession& operator=(const OnnxSession&) = delete;

    bool load(const OnnxRuntime& runtime, const std::filesystem::path& modelPath,
              bool useDml, std::string* error);
    void requestTerminate();
    void clearTerminate();
    bool run(const std::vector<float>& input, int height, int width,
             std::vector<float>& output, int& outputHeight, int& outputWidth,
             std::string* error);

    const ModelInfo& info() const { return info_; }

private:
    void release();

    const OrtApi* api_ = nullptr;
    OrtEnv* env_ = nullptr;
    OrtSession* session_ = nullptr;
    OrtMemoryInfo* memoryInfo_ = nullptr;
    std::atomic<OrtRunOptions*> runOptions_{nullptr};
    std::string inputName_;
    std::string outputName_;
    ModelInfo info_;
    int inputType_ = 0;
};

} // namespace ml