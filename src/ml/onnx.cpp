#include "ml/onnx.h"

#include <windows.h>

#include <cstring>

#ifndef _Frees_ptr_opt_
#define _Frees_ptr_opt_
#endif
#include "onnxruntime_c_api.h"

namespace ml {
namespace {

std::string statusText(const OrtApi* api, OrtStatus* status) {
    if (!status) return {};
    const char* message = api->GetErrorMessage(status);
    const std::string text = message ? message : "ONNX Runtime error";
    api->ReleaseStatus(status);
    return text;
}

} // namespace

OnnxRuntime::~OnnxRuntime() {
    if (module_) {
        FreeLibrary(static_cast<HMODULE>(module_));
        module_ = nullptr;
    }
    api_ = nullptr;
    dmlAppend_ = nullptr;
}

bool OnnxRuntime::load(const std::filesystem::path& dllPath, std::string* error) {
    if (api_) return true;

    HMODULE module = LoadLibraryExW(
        dllPath.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) {
        if (error) *error = "onnxruntime.dll not found: " + dllPath.string();
        return false;
    }

    using GetApiBaseFn = const OrtApiBase*(ORT_API_CALL*)();
    FARPROC symbol = GetProcAddress(module, "OrtGetApiBase");
    if (!symbol) {
        FreeLibrary(module);
        if (error) *error = "onnxruntime.dll has no OrtGetApiBase.";
        return false;
    }
    GetApiBaseFn getApiBase = nullptr;
    static_assert(sizeof(getApiBase) == sizeof(symbol));
    std::memcpy(&getApiBase, &symbol, sizeof(symbol));

    const OrtApi* api = getApiBase()->GetApi(ORT_API_VERSION);
    if (!api) {
        FreeLibrary(module);
        if (error) *error = "ONNX Runtime API version mismatch.";
        return false;
    }

    module_ = module;
    api_ = api;
    FARPROC dmlSymbol =
        GetProcAddress(module, "OrtSessionOptionsAppendExecutionProvider_DML");
    static_assert(sizeof(dmlAppend_) == sizeof(dmlSymbol));
    std::memcpy(&dmlAppend_, &dmlSymbol, sizeof(dmlSymbol));
    return true;
}

OnnxSession::~OnnxSession() {
    release();
}

OrtStatus* OnnxRuntime::appendDml(OrtSessionOptions* options) const {
    return dmlAppend_ ? dmlAppend_(options, 0) : nullptr;
}

void OnnxSession::release() {
    if (api_) {
        if (session_) api_->ReleaseSession(session_);
        OrtRunOptions* runOptions = runOptions_.exchange(nullptr);
        if (runOptions) api_->ReleaseRunOptions(runOptions);
        if (memoryInfo_) api_->ReleaseMemoryInfo(memoryInfo_);
        if (env_) api_->ReleaseEnv(env_);
    }
    session_ = nullptr;
    memoryInfo_ = nullptr;
    env_ = nullptr;
    api_ = nullptr;
    inputName_.clear();
    outputName_.clear();
}

bool OnnxSession::load(const OnnxRuntime& runtime,
                       const std::filesystem::path& modelPath, bool useDml,
                       std::string* error) {
    release();
    api_ = runtime.api();
    if (!api_) {
        if (error) *error = "ONNX Runtime is not loaded.";
        return false;
    }

    OrtStatus* status = api_->CreateEnv(ORT_LOGGING_LEVEL_WARNING,
                                        "ImRelighter", &env_);
    if (status) {
        if (error) *error = statusText(api_, status);
        release();
        return false;
    }

    OrtSessionOptions* options = nullptr;
    status = api_->CreateSessionOptions(&options);
    if (status) {
        if (error) *error = statusText(api_, status);
        release();
        return false;
    }
    status = api_->SetSessionGraphOptimizationLevel(options, ORT_ENABLE_ALL);
    if (status) {
        if (error) *error = statusText(api_, status);
        api_->ReleaseSessionOptions(options);
        release();
        return false;
    }
    status = api_->SetIntraOpNumThreads(options, useDml ? 1 : 4);
    if (status) {
        if (error) *error = statusText(api_, status);
        api_->ReleaseSessionOptions(options);
        release();
        return false;
    }
    if (useDml) {
        if (!runtime.hasDml()) {
            api_->ReleaseSessionOptions(options);
            release();
            if (error) *error = "DirectML execution provider is not available.";
            return false;
        }
        status = runtime.appendDml(options);
        if (status) {
            if (error) *error = statusText(api_, status);
            api_->ReleaseSessionOptions(options);
            release();
            return false;
        }
    }
    status = api_->CreateSession(env_, modelPath.c_str(), options, &session_);
    api_->ReleaseSessionOptions(options);
    if (status) {
        if (error) *error = statusText(api_, status);
        release();
        return false;
    }

    OrtAllocator* allocator = nullptr;
    status = api_->GetAllocatorWithDefaultOptions(&allocator);
    if (status) {
        if (error) *error = statusText(api_, status);
        release();
        return false;
    }

    size_t inputCount = 0;
    size_t outputCount = 0;
    status = api_->SessionGetInputCount(session_, &inputCount);
    if (!status) status = api_->SessionGetOutputCount(session_, &outputCount);
    if (status) {
        if (error) *error = statusText(api_, status);
        release();
        return false;
    }
    if (inputCount != 1 || outputCount != 1) {
        if (error) *error = "Depth model must have one input and one output.";
        release();
        return false;
    }

    char* name = nullptr;
    status = api_->SessionGetInputName(session_, 0, allocator, &name);
    if (status) {
        if (error) *error = statusText(api_, status);
        release();
        return false;
    }
    inputName_ = name;
    OrtStatus* freeStatus = api_->AllocatorFree(allocator, name);
    if (freeStatus) api_->ReleaseStatus(freeStatus);

    name = nullptr;
    status = api_->SessionGetOutputName(session_, 0, allocator, &name);
    if (status) {
        if (error) *error = statusText(api_, status);
        release();
        return false;
    }
    outputName_ = name;
    freeStatus = api_->AllocatorFree(allocator, name);
    if (freeStatus) api_->ReleaseStatus(freeStatus);

    status = api_->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault,
                                       &memoryInfo_);
    if (status) {
        if (error) *error = statusText(api_, status);
        release();
        return false;
    }

    OrtRunOptions* runOptions = nullptr;
    status = api_->CreateRunOptions(&runOptions);
    if (status) {
        if (error) *error = statusText(api_, status);
        release();
        return false;
    }
    runOptions_.store(runOptions);

    return true;
}

void OnnxSession::requestTerminate() {
    OrtRunOptions* runOptions = runOptions_.load();
    if (api_ && runOptions) {
        if (OrtStatus* status = api_->RunOptionsSetTerminate(runOptions))
            api_->ReleaseStatus(status);
    }
}

void OnnxSession::clearTerminate() {
    OrtRunOptions* runOptions = runOptions_.load();
    if (api_ && runOptions) {
        if (OrtStatus* status = api_->RunOptionsUnsetTerminate(runOptions))
            api_->ReleaseStatus(status);
    }
}

bool OnnxSession::run(const std::vector<float>& input, int height, int width,
                      std::vector<float>& output, int& outputHeight,
                      int& outputWidth, std::string* error) {
    if (!api_ || !session_ || !memoryInfo_) {
        if (error) *error = "ONNX session is not loaded.";
        return false;
    }

    const int64_t shape[4] = {1, 3, height, width};
    OrtValue* inputValue = nullptr;
    OrtStatus* status = api_->CreateTensorWithDataAsOrtValue(
        memoryInfo_, const_cast<float*>(input.data()),
        input.size() * sizeof(float), shape, 4,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &inputValue);
    if (status) {
        if (error) *error = statusText(api_, status);
        return false;
    }

    const char* inputNames[1] = {inputName_.c_str()};
    const char* outputNames[1] = {outputName_.c_str()};
    const OrtValue* inputs[1] = {inputValue};
    OrtValue* outputs[1] = {nullptr};
    OrtRunOptions* runOptions = runOptions_.load();
    status = api_->Run(session_, runOptions, inputNames, inputs, 1, outputNames,
                       1, outputs);
    api_->ReleaseValue(inputValue);
    if (status) {
        if (error) *error = statusText(api_, status);
        return false;
    }

    OrtValue* outputValue = outputs[0];
    OrtTensorTypeAndShapeInfo* info = nullptr;
    status = api_->GetTensorTypeAndShape(outputValue, &info);
    if (status) {
        if (error) *error = statusText(api_, status);
        api_->ReleaseValue(outputValue);
        return false;
    }

    size_t dimensions = 0;
    status = api_->GetDimensionsCount(info, &dimensions);
    if (status) {
        if (error) *error = statusText(api_, status);
        api_->ReleaseTensorTypeAndShapeInfo(info);
        api_->ReleaseValue(outputValue);
        return false;
    }
    std::vector<int64_t> shapeOut(dimensions);
    status = api_->GetDimensions(info, shapeOut.data(), dimensions);
    if (status) {
        if (error) *error = statusText(api_, status);
        api_->ReleaseTensorTypeAndShapeInfo(info);
        api_->ReleaseValue(outputValue);
        return false;
    }
    if (dimensions < 2) {
        api_->ReleaseTensorTypeAndShapeInfo(info);
        api_->ReleaseValue(outputValue);
        if (error) *error = "Unexpected depth output shape.";
        return false;
    }

    ONNXTensorElementDataType elementType = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
    status = api_->GetTensorElementType(info, &elementType);
    if (status || elementType != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
        if (status) {
            if (error) *error = statusText(api_, status);
        } else if (error) {
            *error = "Depth output must be float32.";
        }
        api_->ReleaseTensorTypeAndShapeInfo(info);
        api_->ReleaseValue(outputValue);
        return false;
    }

    outputHeight = static_cast<int>(shapeOut[dimensions - 2]);
    outputWidth = static_cast<int>(shapeOut[dimensions - 1]);
    size_t count = 0;
    status = api_->GetTensorShapeElementCount(info, &count);
    if (status || outputHeight <= 0 || outputWidth <= 0 ||
        count != static_cast<size_t>(outputHeight) * outputWidth) {
        if (status) {
            if (error) *error = statusText(api_, status);
        } else if (error) {
            *error = "Unexpected depth output shape.";
        }
        api_->ReleaseTensorTypeAndShapeInfo(info);
        api_->ReleaseValue(outputValue);
        return false;
    }
    void* data = nullptr;
    status = api_->GetTensorMutableData(outputValue, &data);
    api_->ReleaseTensorTypeAndShapeInfo(info);
    if (status) {
        if (error) *error = statusText(api_, status);
        api_->ReleaseValue(outputValue);
        return false;
    }

    const float* values = static_cast<const float*>(data);
    output.assign(values, values + count);
    api_->ReleaseValue(outputValue);
    return true;
}

} // namespace ml