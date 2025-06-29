#pragma once

#include <atomic>
#include <string>

#include "atom/macro.hpp"

enum class FrameType { FITS, NATIVE, XISF, JPG, PNG, TIFF };

enum class UploadMode { CLIENT, LOCAL, BOTH, CLOUD };

struct AtomCameraFrame {
    // 图像尺寸参数
    struct Resolution {
        int width{0};
        int height{0};
        int maxWidth{0};
        int maxHeight{0};
    } ATOM_ALIGNAS(16) resolution;

    struct Binning {
        int horizontal{1};
        int vertical{1};
    } ATOM_ALIGNAS(8) binning;

    // 像素参数
    struct Pixel {
        double size{0.0};
        double sizeX{0.0};
        double sizeY{0.0};
        double depth{0.0};
    } ATOM_ALIGNAS(32) pixel;

    // 帧格式
    FrameType type{FrameType::FITS};
    std::string format;
    UploadMode uploadMode{UploadMode::LOCAL};
    std::atomic_bool isFastread{false};

    // Recent Image
    std::string recentImagePath;
    // 图像数据指针和长度
    void* data{nullptr};
    size_t size{0};
} ATOM_ALIGNAS(128);
