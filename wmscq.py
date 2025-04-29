import csv
import os
from PIL import Image, ImageDraw, ImageFont, ImageOps

missing_files = [] # 初始化一个列表来记录未找到的文件
# removed skipped_files_size list

# 打开CSV文件
# 注意：检查 list.csv 的实际编码，如果不是 utf-8，请修改为正确的编码，例如 'gbk'
try:
    # 推荐使用 utf-8-sig 来处理可能带有 BOM 的 UTF-8 文件
    with open('list.csv', 'r') as file:
        reader = csv.reader(file)
        try:
            next(reader)  # 跳过标题行
        except StopIteration:
            print("错误：CSV 文件 'list.csv' 为空或只有标题行。")
            input("按任意键退出.")
            exit()


        # 遍历CSV文件的每一行
        for idx, row in enumerate(reader):
            # 确保行中有足够的列
            if len(row) < 4:
                print(f"警告：第 {idx + 2} 行数据不完整，跳过。行内容：{row}")
                continue

            # 获取基础文件名和要插入的内容
            base_filename = row[0].strip() # 添加 strip() 去除可能的首尾空格
            brand = row[1].strip()
            name = row[2].strip()
            model = row[3].strip()

            # 定义可能的扩展名和变体
            possible_extensions = ['.jpeg', '.png', '.jpg']
            possible_variants = ['', '.主图']
            found_image_filename = None

            # 尝试查找文件
            for variant in possible_variants:
                for ext in possible_extensions:
                    potential_filename = f"{base_filename}{variant}{ext}"
                    if os.path.exists(potential_filename):
                        found_image_filename = potential_filename
                        break # 找到文件，跳出内层循环
                if found_image_filename:
                    break # 找到文件，跳出外层循环


            # Check if a file was found
            if not found_image_filename:
                print(f"警告：未找到与 '{base_filename}' 相关的文件. 跳过此行.")
                missing_files.append(base_filename) # 记录未找到的文件名
                continue # 继续处理下一行

            # --- 文件找到后的处理逻辑 ---
            image = None # 初始化 image 变量
            new_image = None # 初始化 new_image 变量
            try:
                # 创建保存文件的文件夹
                if not os.path.exists('output'):
                    os.makedirs('output')

                # 打开原始图像
                image = Image.open(found_image_filename)
                # 确保图像是 RGB 模式，以避免处理 PNG 透明度等问题
                if image.mode != 'RGB':
                    image = image.convert('RGB')

                original_width, original_height = image.size

                # --- 修改：判断和处理图像尺寸 ---
                target_dimension = 800
                current_width, current_height = original_width, original_height

                # 1. 如果图片大于 800x800，则按比例缩小
                if original_width > target_dimension or original_height > target_dimension:
                    print(f"信息：文件 '{found_image_filename}' 尺寸 ({original_width}x{original_height}) 大于 {target_dimension}x{target_dimension}，将按比例缩小。")
                    # 使用 thumbnail 方法缩小图像，它会保持宽高比
                    # Image.Resampling.LANCZOS 通常用于高质量缩小
                    image.thumbnail((target_dimension, target_dimension), Image.Resampling.LANCZOS)
                    current_width, current_height = image.size # 获取缩小后的尺寸
                    print(f"信息：已缩小至 {current_width}x{current_height}。")


                # 2. 准备 800x800 的基础画布，并将缩小后或原始尺寸 <= 800 的图片居中粘贴
                # 这一步确保我们总是有一个 800x800 的图片区域用于后续处理
                image_base_800 = Image.new('RGB', (target_dimension, target_dimension), (255, 255, 255))
                paste_x = (target_dimension - current_width) // 2
                paste_y = (target_dimension - current_height) // 2
                image_base_800.paste(image, (paste_x, paste_y))

                # 关闭原始（或缩小后）的 image 对象，因为我们现在使用 image_base_800
                image.close()
                image_for_table = image_base_800 # 使用这个 800x800 的图像进行后续操作
                width, height = target_dimension, target_dimension # 更新宽高为 800x800
                # --- 尺寸处理结束 ---


                # --- 新增：保存主图 ---
                # 从找到的文件名中提取原始扩展名
                _, original_ext_part = os.path.splitext(found_image_filename)
                # 构建主图文件名 (使用 CSV 中的 base_filename)
                main_image_filename_base = f"{base_filename}.主图{original_ext_part}"
                main_image_output_path = os.path.join("output", main_image_filename_base)
                try:
                    # 保存 image_base_800 (也就是即将成为 image_for_table 的图像)
                    image_base_800.save(main_image_output_path)
                    print(f"信息：已保存主图到 '{main_image_output_path}'")
                except Exception as save_err:
                    print(f"警告：保存主图 '{main_image_output_path}' 时出错: {save_err}")
                # --- 主图保存结束 ---


                # --- 后续处理使用 image_for_table (尺寸为 800x800) ---

                # 计算最终画布高度和表格位置 (基于 800x800 图像)
                final_height = 1000
                paste_y = 0 # 图像粘贴在顶部
                table_y = 820 # 固定表格位置

                # 创建新的空白图像 (宽度为 800)
                new_image = Image.new('RGB', (width, final_height), (255, 255, 255))

                # 将 800x800 的图像粘贴到新图像的上方
                new_image.paste(image_for_table, (0, paste_y))

                # 关闭 image_for_table，因为它已经被粘贴了
                image_for_table.close()

                # 创建绘图对象
                draw = ImageDraw.Draw(new_image)

                # 指定字体和字体大小
                try:
                    # 尝试使用相对路径或绝对路径加载字体
                    font_path = "msyh.ttc" # 假设字体在脚本同目录
                    if not os.path.exists(font_path):
                         # 如果不在同目录，尝试系统字体路径 (Windows)
                         font_path = os.path.join(os.environ.get('WINDIR', 'C:\\Windows'), 'Fonts', 'msyh.ttc')
                    font = ImageFont.truetype(font_path, 20)
                except IOError:
                    print(f"警告：未找到字体 'msyh.ttc' (尝试了脚本目录和系统字体目录)，将使用默认字体。")
                    font = ImageFont.load_default()


                # 定义位置和尺寸 (宽度基于 800)
                table_x = 50
                table_width = width - 100 # width 现在是 800
                table_height = 120


                # 绘制表格线和底色
                draw.rectangle((table_x, table_y, table_x + table_width, table_y + table_height), fill=(204, 229, 255))
                draw.rectangle((table_x, table_y, table_x + table_width, table_y + table_height), outline=(0, 0, 0))
                draw.line((table_x, table_y + 40, table_x + table_width, table_y + 40), fill=(0, 0, 0), width=1)
                draw.line((table_x, table_y + 80, table_x + table_width, table_y + 80), fill=(0, 0, 0), width=1)
                draw.line((table_x + 80, table_y, table_x + 80, table_y + table_height), fill=(0, 0, 0), width=1)


                # 在表格中绘制抬头
                draw.text((table_x + 20, table_y + 5), "品牌", fill=(0, 0, 0), font=font)
                draw.text((table_x + 20, table_y + 85), "型号", fill=(0, 0, 0), font=font)
                draw.text((table_x + 20, table_y + 45), "名称", fill=(0, 0, 0), font=font)


                # 在表格中绘制内容
                draw.text((table_x + 100, table_y + 5), brand, fill=(0, 0, 0), font=font)
                draw.text((table_x + 100, table_y + 85), model, fill=(0, 0, 0), font=font)
                draw.text((table_x + 100, table_y + 45), name, fill=(0, 0, 0), font=font)


                # 生成新文件名
                # 从找到的文件名中提取基础名和扩展名
                name_part, ext_part = os.path.splitext(found_image_filename)
                # 如果文件名包含 .主图. ，则移除它
                if ".主图" in name_part:
                     name_part = name_part.replace(".主图", "")
                new_filename = f"{name_part}.图册{ext_part}"


                # 打印处理消息
                print(f"处理文件 {found_image_filename} -> {new_filename}")

                # 保存修改后的图像到 "output" 文件夹中
                output_filename = os.path.join("output", os.path.basename(new_filename))

                # 保存修改后的图像
                new_image.save(output_filename)

                # 关闭图像文件，释放资源 (image 和 image_for_table 已关闭)
                new_image.close()

            except Exception as e:
                print(f"处理文件 '{found_image_filename}' 时发生错误: {e}. 跳过此文件.")
                # 将错误添加到 missing_files 列表，以便最终报告
                missing_files.append(f"{base_filename} (处理时出错: {e})")
                # 确保即使出错也关闭可能已打开的图像
                if image and not image.closed: # 检查 image 是否已定义且未关闭
                    try: image.close()
                    except Exception: pass
                if 'image_for_table' in locals() and image_for_table and not image_for_table.closed: # 检查 image_for_table 是否已定义且未关闭
                    try: image_for_table.close()
                    except Exception: pass
                if new_image and not new_image.closed: # 检查 new_image 是否已定义且未关闭
                    try: new_image.close()
                    except Exception: pass
                continue # 继续处理下一行

except FileNotFoundError:
    print("错误：未找到 CSV 文件 'list.csv'。请确保该文件在脚本所在目录下。")
except Exception as e:
    print(f"读取或处理 CSV 文件时发生严重错误: {e}")

# --- 循环结束后 ---

print("\n--- 处理总结 ---")
# 计算成功处理的文件数 (需要确保 idx 在循环至少运行一次后存在)
total_rows_processed = idx + 1 if 'idx' in locals() else 0
processed_successfully = total_rows_processed - len(missing_files)
print(f"尝试处理行数: {total_rows_processed}")
print(f"成功生成文件数: {processed_successfully}")


# 检查是否有未找到或处理出错的文件
if missing_files:
    print("\n以下文件未能找到或处理时发生错误：")
    for missing in missing_files:
        print(f"- {missing}")


if not missing_files:
    print("\n所有找到的文件均已尝试处理完成。") # 修改总结语

input("按任意键退出.")
