#pragma once

#include <android/asset_manager.h>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "core/types.h"
#include "onnxruntime_cxx_api.h"

namespace imrelighter::ml {

// Runs depth inference on a dedicated worker thread. The rendering thread
// submits the latest image and polls for a result, so the UI never blocks on
// ONNX Runtime. A newer submission replaces any pending job.
class InferenceService {
public:
    explicit InferenceService(AAssetManager* assets);
    ~InferenceService();

    InferenceService(const InferenceService&) = delete;
    InferenceService& operator=(const InferenceService&) = delete;

    void start();
    void stop();

    // The model asset is loaded lazily and cached until the asset path changes.
    void submit(ImageData image, std::string model_id, std::string model_asset_path,
                uint64_t generation);

    bool take_result(DepthResult& result);
    bool take_error(std::string& error, uint64_t& generation);
    bool busy() const;
    std::string loaded_model_id() const;

private:
    struct Job {
        ImageData image;
        std::string model_id;
        std::string model_asset_path;
        uint64_t generation = 0;
    };

    void run();
    void process(Job job);
    bool ensure_session(const std::string& model_asset_path, std::string& error);
    void clear_session();
    void set_error(uint64_t generation, std::string error);

    AAssetManager* assets_ = nullptr;

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::thread worker_;
    bool running_ = false;
    bool processing_ = false;
    bool has_pending_ = false;
    bool has_result_ = false;
    bool has_error_ = false;

    Job pending_;
    DepthResult result_;
    std::string error_;
    uint64_t error_generation_ = 0;

    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::string loaded_model_path_;
    std::string loaded_model_id_;
};

}  // namespace imrelighter::ml
