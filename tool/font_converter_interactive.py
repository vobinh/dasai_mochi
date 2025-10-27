import os
import tkinter as tk
from tkinter import filedialog
from PIL import Image, ImageDraw, ImageFont

def get_font_path_with_gui():
    """Mở cửa sổ đồ họa để người dùng chọn file font .ttf."""
    root = tk.Tk()
    root.withdraw()  # Ẩn cửa sổ tkinter chính không cần thiết
    
    font_path = filedialog.askopenfilename(
        title="Bước 1: Chọn file font TrueType (.ttf)",
        filetypes=[("TrueType Fonts", "*.ttf"), ("All files", "*.*")]
    )
    return font_path

def generate_font_header(font_file, font_size, characters, output_file, font_variable_name):
    """
    Hàm chính để tạo file header font chữ tùy chỉnh.
    (Nội dung hàm này không thay đổi so với các phiên bản trước)
    """
    print("\n-----------------------------------------")
    print(f"Bắt đầu xử lý với các thông số sau:")
    print(f"  - Font: {font_file}")
    print(f"  - Kích thước: {font_size}px")
    print(f"  - File output: {output_file}")
    print("-----------------------------------------")

    try:
        font = ImageFont.truetype(font_file, font_size)
    except IOError:
        print(f"LỖI: Không tìm thấy hoặc không thể mở file font '{font_file}'.")
        return

    unique_chars = sorted(list(set(characters)))
    ref_ascent, ref_descent = font.getmetrics()
    font_y_advance = ref_ascent + ref_descent
    all_bitmaps = bytearray()
    all_glyphs = []
    bitmap_offset = 0

    print(f"Tổng số ký tự cần xử lý: {len(unique_chars)}")

    for i, char in enumerate(unique_chars):
        try:
            bbox = font.getbbox(char)
        except TypeError: bbox = (0,0,0,0)
        if bbox is None: bbox = (0,0,0,0)
            
        left, top, right, bottom = bbox
        width, height = right - left, bottom - top
        x_offset, y_offset = left, -top
        
        try:
            x_advance = font.getlength(char)
        except TypeError: x_advance = 0

        char_image = Image.new('1', (width, height), 0)
        draw = ImageDraw.Draw(char_image)
        draw.text((-left, -top), char, font=font, fill=1)
        
        bitmap_data = bytearray()
        pixels = list(char_image.getdata())
        byte_val = 0
        bit_pos = 0
        for pixel in pixels:
            if pixel > 0: byte_val |= (1 << (7 - bit_pos))
            bit_pos += 1
            if bit_pos == 8:
                bitmap_data.append(byte_val)
                byte_val, bit_pos = 0, 0
        if bit_pos > 0: bitmap_data.append(byte_val)
            
        glyph = {'char_code': ord(char), 'bitmap_offset': bitmap_offset, 'width': width, 'height': height, 'x_advance': int(x_advance), 'x_offset': x_offset, 'y_offset': y_offset}
        all_glyphs.append(glyph)
        all_bitmaps.extend(bitmap_data)
        bitmap_offset += len(bitmap_data)

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(f'#ifndef _{font_variable_name.upper()}_H_\n#define _{font_variable_name.upper()}_H_\n\n#include <Adafruit_GFX.h>\n\n')
        f.write(f'const uint8_t {font_variable_name}Bitmaps[] PROGMEM = {{\n  ')
        for i, byte in enumerate(all_bitmaps):
            f.write(f'0x{byte:02X}, ')
            if (i + 1) % 16 == 0: f.write('\n  ')
        f.write('\n};\n\n')
        all_glyphs.sort(key=lambda g: g['char_code'])
        first_char_code, last_char_code = all_glyphs[0]['char_code'], all_glyphs[-1]['char_code']
        f.write(f'const GFXglyph {font_variable_name}Glyphs[] PROGMEM = {{\n')
        glyph_map = {g['char_code']: g for g in all_glyphs}
        for char_code in range(first_char_code, last_char_code + 1):
            glyph = glyph_map.get(char_code)
            if glyph:
                f.write(f'  {{ {glyph["bitmap_offset"]}, {glyph["width"]}, {glyph["height"]}, {glyph["x_advance"]}, {glyph["x_offset"]}, {glyph["y_offset"]} }}, // 0x{glyph["char_code"]:02X} \'{chr(glyph["char_code"])}\'\n')
            else:
                f.write(f'  {{ 0, 0, 0, 0, 0, 0 }}, // Missing char 0x{char_code:02X}\n')
        f.write('};\n\n')
        f.write(f'const GFXfont {font_variable_name} PROGMEM = {{\n  (uint8_t  *){font_variable_name}Bitmaps,\n  (GFXglyph *){font_variable_name}Glyphs,\n  {first_char_code}, {last_char_code}, {font_y_advance}\n}};\n\n')
        f.write(f'#endif // _{font_variable_name.upper()}_H_\n')

    print(f"\n🚀 Hoàn thành! Đã tạo file '{output_file}' thành công.")
    print(f"   Tổng dung lượng bitmap: {len(all_bitmaps)} bytes")

if __name__ == '__main__':
    # Bước 1: Mở cửa sổ chọn file font
    font_path = get_font_path_with_gui()

    if not font_path:
        print("Không có file font nào được chọn. Đã hủy.")
    else:
        print(f"Đã chọn font: {font_path}")
        
        # Bước 2: Nhập chuỗi ký tự
        chars_to_export = input("Bước 2: Nhập tất cả ký tự cần lấy: ")
        
        # Bước 3: Nhập kích thước font
        while True:
            try:
                font_size_str = input("Bước 3: Nhập kích thước font (ví dụ: 24): ")
                font_size = int(font_size_str)
                break # Thoát vòng lặp nếu nhập số hợp lệ
            except ValueError:
                print("Lỗi: Vui lòng nhập một con số nguyên.")

        # Bước 4: Nhập tên file output
        output_filename = input("Bước 4: Nhập tên file output (ví dụ: MyFont.h): ")
        if not output_filename.endswith('.h'):
            output_filename += '.h'

        # Tự động tạo tên biến từ tên file
        font_var_name = os.path.basename(output_filename).replace('.', '_')
        
        # Gọi hàm xử lý chính
        generate_font_header(font_path, font_size, chars_to_export, output_filename, font_var_name)