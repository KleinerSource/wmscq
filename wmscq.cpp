#define cimg_display 0 // 不需要 CImg 显示功能，避免 X11 依赖 (如果不需要预览)
#include "CImg.h"      // 假设 CImg.h 在包含路径中或同目录下
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem> // 需要 C++17
#include <stdexcept>
#include <algorithm> // for std::min/max if needed, and string operations

// CImg 命名空间
using namespace cimg_library;
namespace fs = std::filesystem; // 文件系统命名空间别名

// --- 辅助函数：分割字符串 ---
std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        // 处理 CSV 可能包含带引号的字段（简单处理）
        if (!token.empty() && token.front() == '"' && token.back() == '"') {
            token = token.substr(1, token.length() - 2);
            // 可能需要处理引号内的转义引号，这里简化了
        }
        tokens.push_back(token);
    }
    return tokens;
}

// --- 辅助函数：去除字符串首尾空格 ---
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, (last - first + 1));
}

// --- 辅助函数：替换字符串中的子串 ---
void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}


int main() {
    std::vector<std::string> missing_files; // 记录未找到或处理出错的文件
    int line_number = 0; // Declare line_number outside the try block

    try {
        std::ifstream file("list.csv");
        if (!file.is_open()) {
            std::cerr << "错误：无法打开 CSV 文件 'list.csv'。" << std::endl;
            std::cerr << "按任意键退出." << std::endl;
            std::cin.get();
            return 1;
        }

        std::string line;
        // line_number = 0; // Initialization moved outside

        // 跳过标题行
        if (!std::getline(file, line)) {
             std::cerr << "错误：CSV 文件 'list.csv' 为空或只有标题行。" << std::endl;
             std::cerr << "按任意键退出." << std::endl;
             std::cin.get();
             return 1;
        }
        line_number++; // Increment after reading the header

        // 遍历CSV文件的每一行
        while (std::getline(file, line)) {
            line_number++; // Increment for each data row
            std::vector<std::string> row = split(line, ','); // 假设是逗号分隔

            if (row.size() < 4) {
                std::cerr << "警告：第 " << line_number << " 行数据不完整，跳过。行内容：" << line << std::endl;
                continue;
            }

            // 获取基础文件名和要插入的内容
            std::string base_filename = trim(row[0]);
            std::string brand = trim(row[1]);
            std::string name = trim(row[2]);
            std::string model = trim(row[3]);

            // 定义可能的扩展名和变体
            const std::vector<std::string> possible_extensions = {".jpeg", ".png", ".jpg"};
            const std::vector<std::string> possible_variants = {"", ".主图"};
            fs::path found_image_path; // 使用 path 对象
            bool file_found = false;

            // 尝试查找文件
            for (const auto& variant : possible_variants) {
                for (const auto& ext : possible_extensions) {
                    fs::path potential_path = base_filename + variant + ext;
                    if (fs::exists(potential_path)) {
                        found_image_path = potential_path;
                        file_found = true;
                        break;
                    }
                }
                if (file_found) break;
            }

            // 检查是否找到文件
            if (!file_found) {
                std::cerr << "警告：未找到与 '" << base_filename << "' 相关的文件. 跳过此行." << std::endl;
                missing_files.push_back(base_filename);
                continue;
            }

            std::string found_image_filename = found_image_path.string(); // 获取字符串路径

            // --- 文件找到后的处理逻辑 ---
            CImg<unsigned char> image; // 使用 CImg 对象
            CImg<unsigned char> new_image;

            try {
                // 创建保存文件的文件夹
                fs::path output_dir = "output";
                if (!fs::exists(output_dir)) {
                    fs::create_directories(output_dir);
                }

                // 打开原始图像
                image.load(found_image_filename.c_str());

                // 确保图像是 RGB 模式 (CImg 通常内部处理，但检查谱数)
                if (image.spectrum() == 4) { // 例如 RGBA
                    image.channels(0, 2); // 保留 R, G, B 通道
                } else if (image.spectrum() == 2) { // 例如灰度+Alpha
                     image.channels(0,0); // 转为灰度，再转RGB可能更好
                     image.resize(-100,-100,-100,3); // 尝试扩展到3通道
                } else if (image.spectrum() == 1) { // 灰度
                     image.resize(-100,-100,-100,3); // 扩展到3通道
                } else if (image.spectrum() != 3) {
                     throw std::runtime_error("不支持的图像通道数: " + std::to_string(image.spectrum()));
                }


                int original_width = image.width();
                int original_height = image.height();

                // --- 处理图像尺寸 ---
                const int target_dimension = 800;
                int current_width = original_width;
                int current_height = original_height;

                // 1. 如果图片大于 800x800，则按比例缩小
                if (original_width > target_dimension || original_height > target_dimension) {
                    std::cout << "信息：文件 '" << found_image_filename << "' 尺寸 (" << original_width << "x" << original_height << ") 大于 " << target_dimension << "x" << target_dimension << "，将按比例缩小。" << std::endl;

                    // 计算缩放比例，保持宽高比
                    float ratio = std::min((float)target_dimension / original_width, (float)target_dimension / original_height);
                    int new_width = static_cast<int>(original_width * ratio);
                    int new_height = static_cast<int>(original_height * ratio);

                    // 使用 CImg 的 resize (6 = Lanczos插值)
                    image.resize(new_width, new_height, -100, -100, 6);

                    current_width = image.width();
                    current_height = image.height();
                    std::cout << "信息：已缩小至 " << current_width << "x" << current_height << "。" << std::endl;
                }

                // 2. 准备 800x800 的基础画布，并将缩小后或原始尺寸 <= 800 的图片居中粘贴
                CImg<unsigned char> image_base_800(target_dimension, target_dimension, 1, 3, 255); // 深度1, 3通道RGB, 填充白色
                int paste_x = (target_dimension - current_width) / 2;
                int paste_y = (target_dimension - current_height) / 2;
                image_base_800.draw_image(paste_x, paste_y, image); // 粘贴图像

                // image 对象不再需要，image_base_800 是我们处理的基础
                int width = target_dimension;
                int height = target_dimension;
                // --- 尺寸处理结束 ---


                // --- 后续处理使用 image_base_800 (尺寸为 800x800) ---
                const int final_height = 1000;
                const int table_y_pos = 820; // 固定表格位置

                // 创建新的空白图像 (宽度为 800)
                new_image.assign(width, final_height, 1, 3, 255); // 创建 800x1000 白色画布

                // 将 800x800 的图像粘贴到新图像的上方
                new_image.draw_image(0, 0, image_base_800); // paste_y = 0

                // 定义颜色
                const unsigned char color_bg[] = {204, 229, 255}; // 浅蓝背景
                const unsigned char color_fg[] = {0, 0, 0};       // 黑色前景/线条/文字

                // 定义位置和尺寸 (宽度基于 800)
                const int table_x = 50;
                const int table_width = width - 100; // width 现在是 800
                const int table_height = 120;

                // 绘制表格线和底色
                new_image.draw_rectangle(table_x, table_y_pos, table_x + table_width, table_y_pos + table_height, color_bg, 1.0f); // 填充背景
                new_image.draw_rectangle(table_x, table_y_pos, table_x + table_width, table_y_pos + table_height, color_fg, 1.0f, 0xFFFFFFFF); // 绘制边框 (pattern 0xFFFFFFFF)

                // 绘制表格中的横线
                new_image.draw_line(table_x, table_y_pos + 40, table_x + table_width, table_y_pos + 40, color_fg);
                new_image.draw_line(table_x, table_y_pos + 80, table_x + table_width, table_y_pos + 80, color_fg);

                // 绘制表格中的竖线
                new_image.draw_line(table_x + 80, table_y_pos, table_x + 80, table_y_pos + table_height, color_fg);

                // 在表格中绘制抬头和内容 (使用 CImg 默认字体)
                // 注意：字体大小和位置可能需要调整以匹配 CImg 的默认字体
                const int font_size = 20; // CImg 的字体大小参数可能与 PIL 不同
                const unsigned char white[] = {255, 255, 255}; // CImg draw_text 需要背景色

                new_image.draw_text(table_x + 20, table_y_pos + 5, "品牌", color_fg, color_bg, 1.0f, font_size);
                new_image.draw_text(table_x + 20, table_y_pos + 45, "名称", color_fg, color_bg, 1.0f, font_size);
                new_image.draw_text(table_x + 20, table_y_pos + 85, "型号", color_fg, color_bg, 1.0f, font_size);

                new_image.draw_text(table_x + 100, table_y_pos + 5, brand.c_str(), color_fg, color_bg, 1.0f, font_size);
                new_image.draw_text(table_x + 100, table_y_pos + 45, name.c_str(), color_fg, color_bg, 1.0f, font_size);
                new_image.draw_text(table_x + 100, table_y_pos + 85, model.c_str(), color_fg, color_bg, 1.0f, font_size);


                // 生成新文件名
                std::string name_part_str = found_image_path.stem().string(); // 获取文件名（无扩展名）
                std::string ext_part_str = found_image_path.extension().string(); // 获取扩展名

                // 如果文件名包含 .主图. ，则移除它
                replace_all(name_part_str, ".主图", "");

                std::string new_base_filename = name_part_str + ".图册";
                fs::path new_filename_path = new_base_filename + ext_part_str;

                // 打印处理消息
                std::cout << "处理文件 " << found_image_filename << " -> " << new_filename_path.filename().string() << std::endl;

                // 保存修改后的图像到 "output" 文件夹中
                fs::path output_filepath = output_dir / new_filename_path.filename(); // 组合路径

                // 保存修改后的图像
                new_image.save(output_filepath.string().c_str());

            } catch (const CImgException &e) {
                std::cerr << "处理文件 '" << found_image_filename << "' 时发生 CImg 错误: " << e.what() << ". 跳过此文件." << std::endl;
                missing_files.push_back(base_filename + " (处理时CImg出错)");
                continue; // 继续处理下一行
            } catch (const std::exception &e) {
                std::cerr << "处理文件 '" << found_image_filename << "' 时发生错误: " << e.what() << ". 跳过此文件." << std::endl;
                missing_files.push_back(base_filename + " (处理时出错)");
                continue; // 继续处理下一行
            }
        } // end while loop reading csv lines

        file.close();

    } catch (const std::exception &e) {
        std::cerr << "读取或处理 CSV 文件时发生严重错误: " << e.what() << std::endl;
    }

    // --- 循环结束后 ---
    std::cout << "\n--- 处理总结 ---" << std::endl;
    // Now line_number is accessible here
    int total_rows_attempted = line_number > 0 ? line_number - 1 : 0; // 减去标题行
    int processed_successfully = total_rows_attempted - missing_files.size();
    std::cout << "尝试处理行数: " << total_rows_attempted << std::endl;
    std::cout << "成功生成文件数: " << processed_successfully << std::endl;

    // 检查是否有未找到或处理出错的文件
    if (!missing_files.empty()) {
        std::cout << "\n以下文件未能找到或处理时发生错误：" << std::endl;
        for (const auto& missing : missing_files) {
            std::cout << "- " << missing << std::endl;
        }
    } else {
         std::cout << "\n所有找到的文件均已尝试处理完成。" << std::endl;
    }

    std::cout << "按任意键退出." << std::endl;
    std::cin.get(); // 等待用户输入

    return 0;
}