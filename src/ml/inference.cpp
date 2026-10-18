#include "ml/inference.h"

#include <utility>

#include "ml/model_config.h"
#include "ml/preprocess.h"

namespace ml {

InferenceService::~InferenceService() {
    stop();
}

void InferenceService::start(const std::filesystem::path& runtimeDll) {
    runtimeDll_ = runtimeDll;
    if (worker_.joinable()) return;
    worker_ = std::thread(&InferenceService::workerLoop, this);
}

void InferenceService::stop() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        pending_.reset();
    }
    cancelRequested_ = true;
    session_.requestTerminate();
    condition_.notify_all();
    if (worker_.joinable()) worker_.join();
    stopping_ = false;
}

bool InferenceService::submit(InferenceJob job) {
    if (busy_.exchange(true)) return false;
    cancelRequested_ = false;
    session_.clearTerminate();

    {
        std::lock_guard lock(mutex_);
        pending_ = std::move(job);
        result_.reset();
    }
    condition_.notify_one();
    return true;
}

void InferenceService::cancel() {
    if (!busy_.load()) return;
    cancelRequested_ = true;
    session_.requestTerminate();
}

bool InferenceService::poll(InferenceResult& result) {
    std::lock_guard lock(mutex_);
    if (!result_) return false;
    result = std::move(*result_);
    result_.reset();
    return true;
}

void InferenceService::workerLoop() {
    for (;;) {
        InferenceJob job;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [&] { return stopping_ || pending_.has_value(); });
            if (stopping_) break;
            job = std::move(*pending_);
            pending_.reset();
        }

        InferenceResult result;
        result.documentId = job.documentId;
        result.generation = job.generation;

        if (!runtime_.api()) {
            std::string error;
            if (!runtime_.load(runtimeDll_, &error)) {
                result.error = error;
            }
        }

        if (result.error.empty()) {
            if (loadedModel_ != job.modelPath || loadedDml_ != job.useDml) {
                std::string error;
                bool useDml = job.useDml;
                if (!session_.load(runtime_, job.modelPath, useDml, &error)) {
                    if (useDml && job.allowCpuFallback) {
                        useDml = false;
                        error.clear();
                        if (!session_.load(runtime_, job.modelPath, useDml,
                                           &error)) {
                            loadedModel_.clear();
                            loadedDml_ = false;
                            result.error = error;
                        }
                    } else {
                        loadedModel_.clear();
                        loadedDml_ = false;
                        result.error = error;
                    }
                }
                if (result.error.empty()) {
                    loadedModel_ = job.modelPath;
                    loadedDml_ = useDml;
                }
            }
        }

        if (result.error.empty() && !cancelRequested_.load()) {
            ModelConfig modelConfig = loadModelConfig(job.modelPath);
            if (job.inputSize > 0) modelConfig.inputSize = job.inputSize;
            PreprocessResult preprocessed;
            std::string error;
            if (!preprocess(job.rgba.data(), job.width, job.height, preprocessed,
                            &error, modelConfig)) {
                result.error = error;
            } else if (cancelRequested_.load()) {
                result.cancelled = true;
            } else {
                std::vector<float> output;
                int outputHeight = 0;
                int outputWidth = 0;
                if (!session_.run(preprocessed.tensor, preprocessed.height,
                                  preprocessed.width, output, outputHeight,
                                  outputWidth, &error)) {
                    if (cancelRequested_.load()) {
                        result.cancelled = true;
                    } else {
                        result.error = error;
                    }
                } else if (!postprocess(output, outputWidth, outputHeight,
                                        job.width, job.height, result.depth,
                                        &error, modelConfig.invertDepth)) {
                    result.error = error;
                }
            }
        } else if (cancelRequested_.load()) {
            result.cancelled = true;
        }

        if (cancelRequested_.load()) result.cancelled = true;
        if (result.cancelled) {
            result.depth.clear();
            result.error.clear();
        }

        {
            std::lock_guard lock(mutex_);
            result_ = std::move(result);
        }
        cancelRequested_ = false;
        busy_ = false;
    }
}

} // namespace ml