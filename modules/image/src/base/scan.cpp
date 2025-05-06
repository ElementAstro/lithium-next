#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// 图片分类结构体
struct ImageCategory {
    std::string name;
    std::vector<fs::path> images;
};

// 日期目录结构体
struct DateDirectory {
    std::string date;
    std::vector<ImageCategory> categories;
};

// 整体文件夹结构
struct FolderStructure {
    std::vector<DateDirectory> dates;
};

// 判断是否为图片文件
bool is_image_file(const fs::path& path) {
    static const std::unordered_set<std::string> valid_extensions{
        ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp"};

    if (!fs::is_regular_file(path))
        return false;

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return valid_extensions.contains(ext);
}

// 解析文件夹结构
FolderStructure parse_folder_structure(const fs::path& root) {
    FolderStructure result;

    // 第一层：日期目录
    for (const auto& date_entry : fs::directory_iterator(root)) {
        if (!date_entry.is_directory())
            continue;

        DateDirectory date_dir;
        date_dir.date = date_entry.path().filename().string();

        // 第二层：图片分类目录
        for (const auto& category_entry : fs::directory_iterator(date_entry)) {
            if (!category_entry.is_directory())
                continue;

            ImageCategory category;
            category.name = category_entry.path().filename().string();

            // 第三层：图片文件
            for (const auto& img_entry :
                 fs::directory_iterator(category_entry)) {
                if (is_image_file(img_entry)) {
                    category.images.emplace_back(img_entry.path());
                }
            }

            if (!category.images.empty()) {
                date_dir.categories.emplace_back(std::move(category));
            }
        }

        if (!date_dir.categories.empty()) {
            result.dates.emplace_back(std::move(date_dir));
        }
    }

    return result;
}
