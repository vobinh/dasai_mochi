import os
import glob

def format_byte_array(data, var_name):
    """Định dạng dữ liệu byte thành một mảng C++ PROGMEM."""
    hex_string = ", ".join([f"0x{byte:02X}" for byte in data])
    # Chia thành các dòng có tối đa 16 byte để dễ đọc
    lines = []
    # 16 giá trị hex, mỗi giá trị 4 ký tự ("0x", "FF", ", ") -> 16 * 6 = 96. Chọn chunk size an toàn hơn.
    chunk_size = 16 * 6 
    for i in range(0, len(hex_string), chunk_size):
        lines.append("  " + hex_string[i:i+chunk_size])
    
    formatted_string = f"const uint8_t {var_name}[] PROGMEM = {{\n"
    formatted_string += "\n".join(lines)
    # Xóa dấu phẩy và khoảng trắng ở cuối dòng cuối cùng nếu có
    if formatted_string.strip().endswith(","):
        formatted_string = formatted_string.rstrip()
        formatted_string = formatted_string.rstrip(',')
        
    formatted_string += "\n};"
    return formatted_string

def create_header_file(input_folder, output_filename="analog_face.h"):
    """
    Tìm tất cả các file JPG và PNG trong thư mục đầu vào, chuyển đổi chúng,
    và tạo ra một file header C++.
    """
    # *** ĐÃ CẬP NHẬT: Tìm cả file .jpg và .png ***
    search_path_jpg = os.path.join(input_folder, '*.jpg')
    search_path_png = os.path.join(input_folder, '*.png')
    
    image_files_jpg = glob.glob(search_path_jpg)
    image_files_png = glob.glob(search_path_png)
    
    all_image_files = image_files_jpg + image_files_png

    # Sắp xếp tất cả các file theo thứ tự số
    image_files = sorted(all_image_files, key=lambda x: int(os.path.splitext(os.path.basename(x))[0]))

    if not image_files:
        print(f"Lỗi: Không tìm thấy file .jpg hoặc .png nào trong thư mục '{input_folder}'")
        return

    print(f"Đã tìm thấy {len(image_files)} mặt đồng hồ. Đang xử lý...")

    header_content = []
    frame_var_names = []
    frame_size_names = []

    # --- Phần đầu của file header ---
    header_content.append("#pragma once")
    header_content.append("#include <stdint.h>")
    header_content.append("#include <pgmspace.h>\n")
    header_content.append("// File được tạo tự động bởi Tool chuyển đổi Python")
    header_content.append("// Chứa dữ liệu hình nền cho các mặt đồng hồ kim.\n")

    # --- Xử lý từng file ảnh ---
    for i, image_path in enumerate(image_files):
        try:
            with open(image_path, 'rb') as f:
                image_data = f.read()
            
            var_name = f"analog_face_frame_{i}"
            size_name = f"{var_name}_size"

            # Thêm phần định nghĩa mảng byte của ảnh
            header_content.append(f"// --- Dữ liệu Frame {i} ({os.path.basename(image_path)}) ---")
            header_content.append(format_byte_array(image_data, var_name))
            header_content.append(f"const uint16_t {size_name} = sizeof({var_name});\n")

            frame_var_names.append(var_name)
            frame_size_names.append(size_name)
        except Exception as e:
            print(f"Lỗi khi xử lý file {image_path}: {e}")
            continue
    
    # --- Tạo các mảng quản lý ---
    header_content.append("// --- Mảng quản lý các frame ---")
    
    # Mảng con trỏ tới các frame
    header_content.append("const uint8_t* const analog_face_frames[] = {")
    for name in frame_var_names:
        header_content.append(f"  {name},")
    header_content.append("};")

    # Mảng kích thước của các frame
    header_content.append("\nconst uint16_t analog_face_frames_size[] = {")
    for name in frame_size_names:
        header_content.append(f"  {name},")
    header_content.append("};")

    # Biến tổng số frame
    header_content.append(f"\nconst uint16_t analog_face_num_frames = sizeof(analog_face_frames) / sizeof(analog_face_frames[0]);")

    # --- Ghi ra file ---
    try:
        # *** ĐÃ SỬA LỖI: Thêm encoding='utf-8' để xử lý ký tự tiếng Việt ***
        with open(output_filename, 'w', encoding='utf-8') as f:
            f.write("\n".join(header_content))
        print(f"\nThành công! Đã tạo file '{output_filename}' với {len(frame_var_names)} mặt đồng hồ.")
    except Exception as e:
        print(f"Lỗi khi ghi file {output_filename}: {e}")


if __name__ == "__main__":
    folder_path = input("Nhập đường dẫn đến thư mục chứa các mặt đồng hồ (ví dụ: C:/watch_faces): ")
    if os.path.isdir(folder_path):
        create_header_file(folder_path)
    else:
        print("Lỗi: Đường dẫn không hợp lệ hoặc không phải là một thư mục.")

