#include "ml/inference_service.h"

#include <android/log.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <utility>
#include <vector>

#include "io/image_loader.h"
#include "ml/depth_processing.h"
#include "ml/preprocess.h"

#define LOG_TAG "ImRelighterAndroid"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace imrelighter::ml {
namespace {

std::vector<int64_t> resolve_input_shape(const std::vector<int64_t>& model_shape,
                                         int input_width, int input_height) {
    std::vector<int64_t> shape = model_shape;
    if (shape.size() != 4) {
        shape = {1, 3, input_height, input_width};
    } else {
        shape[0] = 1;
        shape[1] = 3;
        shape[2] = input_height;
        shape[3] = input_width;
    }
    return shape;
}

}  // namespace

InferenceService::InferenceService(AAssetManager* assets) : assets_(assets) {}

InferenceService::~InferenceService() {
    stop();
}

void InferenceService::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }
    running_ = true;
    worker_ = std::thread(&InferenceService::run, this);
}

void InferenceService::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    condition_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    processing_ = false;
    has_pending_ = false;
    has_result_ = false;
    has_error_ = false;
    clear_session();
}

void InferenceService::submit(ImageData image, std::string model_id,
                              std::string model_asset_path, uint64_t generation) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        has_error_ = true;
        error_ = "inference service is not running";
        error_generation_ = generation;
        return;
    }
    pending_.image = std::move(image);
    pending_.model_id = std::move(model_id);
    pending_.model_asset_path = std::move(model_asset_path);
    pending_.generation = generation;
    has_pending_ = true;
    has_result_ = false;
    has_error_ = false;
    condition_.notify_one();
}

bool InferenceService::take_result(DepthResult& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_result_) {
        return false;
    }
    result = std::move(result_);
    has_result_ = false;
    return true;
}

bool InferenceService::take_error(std::string& error, uint64_t& generation) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_error_) {
        return false;
    }
    error = error_;
    generation = error_generation_;
    has_error_ = false;
    return true;
}

bool InferenceService::busy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return processing_ || has_pending_;
}

std::string InferenceService::loaded_model_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_model_id_;
}

void InferenceService::run() {
    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return !running_ || has_pending_; });
            if (!running_) {
                return;
            }
            job = std::move(pending_);
            has_pending_ = false;
            processing_ = true;
        }

        process(std::move(job));

        {
            std::lock_guard<std::mutex> lock(mutex_);
            processing_ = false;
        }
    }
}

void InferenceService::process(Job job) {
    std::string error;
    try {
        std::vector<float> tensor;
        int input_width = 0;
        int input_height = 0;
        if (!preprocessImage(job.image, tensor, input_width, input_height, error)) {
            set_error(job.generation, error);
            return;
        }

        if (!ensure_session(job.model_asset_path, error)) {
            set_error(job.generation, error);
            return;
        }

        Ort::AllocatorWithDefaultOptions allocator;
        const auto input_name = session_->GetInputNameAllocated(0, allocator);
        const auto output_name = session_->GetOutputNameAllocated(0, allocator);
        const auto input_type_info = session_->GetInputTypeInfo(0);
        const auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        const auto input_type = input_tensor_info.GetElementType();
        if (input_type != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
            set_error(job.generation, "unsupported model input type; float32 is required");
            return;
        }

        std::vector<int64_t> input_shape =
            resolve_input_shape(input_tensor_info.GetShape(), input_width, input_height);
        size_t expected_elements = 1;
        for (const int64_t dimension : input_shape) {
            expected_elements *= static_cast<size_t>(dimension);
        }
        if (expected_elements != tensor.size()) {
            set_error(job.generation, "preprocessed tensor shape does not match model input");
            return;
        }

        const Ort::MemoryInfo memory_info =
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, tensor.data(), tensor.size(), input_shape.data(), input_shape.size());

        const char* input_names[] = {input_name.get()};
        const char* output_names[] = {output_name.get()};
        const auto start = std::chrono::steady_clock::now();
        auto outputs = session_->Run(Ort::RunOptions{nullptr}, input_names, &input_tensor, 1,
                                     output_names, 1);
        const auto end = std::chrono::steady_clock::now();

        if (outputs.empty() || !outputs[0].IsTensor()) {
            set_error(job.generation, "model returned no tensor");
            return;
        }
        const auto output_info = outputs[0].GetTensorTypeAndShapeInfo();
        if (output_info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
            set_error(job.generation, "unsupported model output type; float32 is required");
            return;
        }

        std::vector<int64_t> output_shape = output_info.GetShape();
        int output_width = input_width;
        int output_height = input_height;
        if (output_shape.size() >= 2) {
            const int64_t width = output_shape[output_shape.size() - 1];
            const int64_t height = output_shape[output_shape.size() - 2];
            if (width > 0 && height > 0) {
                output_width = static_cast<int>(width);
                output_height = static_cast<int>(height);
            }
        }

        const size_t output_elements = output_info.GetElementCount();
        if (output_elements <
            static_cast<size_t>(output_width) * static_cast<size_t>(output_height)) {
            set_error(job.generation, "model output is smaller than its declared shape");
            return;
        }

        DepthResult result;
        std::vector<float> depth_data(output_elements);
        std::copy(outputs[0].GetTensorData<float>(),
                  outputs[0].GetTensorData<float>() + output_elements,
                  depth_data.begin());
        if (!buildDepthAndNormal(depth_data, output_width, output_height,
                                 job.image.width, job.image.height, result, error)) {
            set_error(job.generation, error);
            return;
        }
        result.inference_ms =
            std::chrono::duration<double, std::milli>(end - start).count();
        result.model_id = job.model_id;
        result.generation = job.generation;

        std::lock_guard<std::mutex> lock(mutex_);
        result_ = std::move(result);
        has_result_ = true;
        LOGI("inference done: generation=%llu model=%s %.1f ms depth=%dx%d",
             static_cast<unsigned long long>(job.generation), job.model_id.c_str(),
             result_.inference_ms, result_.width, result_.height);
    } catch (const std::exception& exception) {
        set_error(job.generation, exception.what());
    }
}

bool InferenceService::ensure_session(const std::string& model_asset_path,
                                      std::string& error) {
    if (session_ && loaded_model_path_ == model_asset_path) {
        return true;
    }

    std::vector<uint8_t> model_bytes;
    const bool local_file =
        !model_asset_path.empty() &&
        (model_asset_path.front() == '/' || model_asset_path.rfind("file:", 0) == 0);
    if (local_file) {
        std::string path = model_asset_path;
        if (path.rfind("file:", 0) == 0) {
            path.erase(0, 5);
        }
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) {
            error = "could not open model file: " + path;
            return false;
        }
        const std::streamsize size = input.tellg();
        if (size <= 0) {
            error = "empty model file: " + path;
            return false;
        }
        model_bytes.resize(static_cast<size_t>(size));
        input.seekg(0, std::ios::beg);
        if (!input.read(reinterpret_cast<char*>(model_bytes.data()), size)) {
            error = "could not read model file: " + path;
            return false;
        }
    } else if (!io::readAsset(assets_, model_asset_path, model_bytes, error)) {
        return false;
    }

    try {
        if (!env_) {
            env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING,
                                              "ImRelighterAndroid");
        }
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(4);
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        auto session = std::make_unique<Ort::Session>(
            *env_, model_bytes.data(), model_bytes.size(), options);
        session_ = std::move(session);
        loaded_model_path_ = model_asset_path;
        loaded_model_id_ = model_asset_path;
        LOGI("ORT session loaded from %s (%zu bytes)", model_asset_path.c_str(),
             model_bytes.size());
        return true;
    } catch (const std::exception& exception) {
        error = std::string("could not load model: ") + exception.what();
        return false;
    }
}

void InferenceService::clear_session() {
    session_.reset();
    env_.reset();
    loaded_model_path_.clear();
    loaded_model_id_.clear();
}

void InferenceService::set_error(uint64_t generation, std::string error) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_ = std::move(error);
    error_generation_ = generation;
    has_error_ = true;
    LOGE("inference error: generation=%llu %s",
         static_cast<unsigned long long>(generation), error_.c_str());
}

}  // namespace imrelighter::ml
