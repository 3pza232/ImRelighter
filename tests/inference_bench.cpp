#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "ml/onnx.h"
#include "ml/preprocess.h"

#ifndef IMRELIGHTER_MODEL_PATH
#define IMRELIGHTER_MODEL_PATH "models/depth-anything-v2-small/model.onnx"
#endif
#ifndef IMRELIGHTER_ORT_DLL
#define IMRELIGHTER_ORT_DLL "onnxruntime.dll"
#endif

namespace {

double runOnce(ml::OnnxSession& session, const ml::PreprocessResult& input) {
    std::vector<float> output;
    int outputWidth = 0;
    int outputHeight = 0;
    std::string error;
    const auto begin = std::chrono::steady_clock::now();
    if (!session.run(input.tensor, input.height, input.width, output,
                     outputHeight, outputWidth, &error)) {
        std::printf("run failed: %s\n", error.c_str());
        return -1.0;
    }
    const auto end = std::chrono::steady_clock::now();
    if (output.empty()) return -1.0;
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    constexpr int width = 512;
    constexpr int height = 512;
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 4;
            rgba[i + 0] = static_cast<uint8_t>(x * 255 / (width - 1));
            rgba[i + 1] = static_cast<uint8_t>(y * 255 / (height - 1));
            rgba[i + 2] = 160;
            rgba[i + 3] = 255;
        }
    }

    ml::PreprocessResult input;
    std::string error;
    if (!ml::preprocess(rgba.data(), width, height, input, &error)) {
        std::printf("preprocess failed: %s\n", error.c_str());
        return 1;
    }

    ml::OnnxRuntime runtime;
    if (!runtime.load(IMRELIGHTER_ORT_DLL, &error)) {
        std::printf("runtime load failed: %s\n", error.c_str());
        return 2;
    }

    const char* model = argc > 1 ? argv[1] : IMRELIGHTER_MODEL_PATH;
    ml::OnnxSession cpu;
    if (!cpu.load(runtime, model, false, &error)) {
        std::printf("CPU session failed: %s\n", error.c_str());
        return 3;
    }
    const double cpuWarm = runOnce(cpu, input);
    const double cpuMilliseconds = runOnce(cpu, input);
    if (cpuWarm < 0.0 || cpuMilliseconds < 0.0) return 4;
    std::printf("CPU  %.1f ms (warmup %.1f ms)\n", cpuMilliseconds, cpuWarm);

    if (!runtime.hasDml()) {
        std::printf("DML  unavailable\n");
        return 0;
    }
    ml::OnnxSession dml;
    if (!dml.load(runtime, model, true, &error)) {
        std::printf("DML  unavailable: %s\n", error.c_str());
        return 0;
    }
    const double dmlWarm = runOnce(dml, input);
    const double dmlMilliseconds = runOnce(dml, input);
    if (dmlWarm < 0.0 || dmlMilliseconds < 0.0) return 5;
    std::printf("DML  %.1f ms (warmup %.1f ms)\n", dmlMilliseconds, dmlWarm);
    return 0;
}
