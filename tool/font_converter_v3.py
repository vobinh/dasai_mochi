import os
import argparse
import tkinter as tk
from tkinter import filedialog
from PIL import Image, ImageDraw, ImageFont

def get_font_path_with_gui():
    """Mở cửa sổ đồ họa để người dùng chọn file font .ttf."""
    root = tk.Tk()
    root.withdraw()  # Ẩn cửa sổ tkinter chính không cần thiết
    
    # Mở hộp thoại chọn file
    font_path = filedialog.askopenfilename(
        title="Chọn file font TrueType (.ttf)",
        filetypes=[("TrueType Fonts", "*.ttf"), ("All files", "*.*")]
    )
    
    return font_path

def generate_font_header(font_file, font_size, characters, output_file, font_variable_name):
    """
    Hàm chính để tạo file header font chữ tùy chỉnh.
    """
    # ... (Toàn bộ nội dung hàm này giữ nguyên y hệt như phiên bản v2) ...
    print(f"Bắt đầu xử lý font: {font_file}")
    print(f"Kích thước: {font_size}px")
    print(f"Số ký tự: {len(characters)}")
    print(f"File output: {output_file}")
    print(f"Tên biến: {font_variable_name}")

    try:
        font = ImageFont.truetype(font_file, font_size)
    except IOError:
        print(f"Lỗi: Không tìm thấy hoặc không thể mở file font '{font_file}'.")
        return

    unique_chars = sorted(list(set(characters)))
    ref_ascent, ref_descent = font.getmetrics()
    font_y_advance = ref_ascent + ref_descent
    all_bitmaps = bytearray()
    all_glyphs = []
    bitmap_offset = 0

    for i, char in enumerate(unique_chars):
        try:
            bbox = font.getbbox(char)
        except TypeError:
            bbox = (0,0,0,0)
        if bbox is None:
            bbox = (0,0,0,0)
        left, top, right, bottom = bbox
        width = right - left
        height = bottom - top
        x_offset = left
        y_offset = -top
        try:
            x_advance = font.getlength(char)
        except TypeError:
            x_advance = 0
        char_image = Image.new('1', (width, height), 0)
        draw = ImageDraw.Draw(char_image)
        draw.text((-left, -top), char, font=font, fill=1)
        bitmap_data = bytearray()
        pixels = list(char_image.getdata())
        byte_val = 0
        bit_pos = 0
        for pixel in pixels:
            if pixel > 0:
                byte_val |= (1 << (7 - bit_pos))
            bit_pos += 1
            if bit_pos == 8:
                bitmap_data.append(byte_val)
                byte_val = 0
                bit_pos = 0
        if bit_pos > 0:
            bitmap_data.append(byte_val)
        glyph = {
            'char_code': ord(char), 'bitmap_offset': bitmap_offset,
            'width': width, 'height': height, 'x_advance': int(x_advance),
            'x_offset': x_offset, 'y_offset': y_offset,
        }
        all_glyphs.append(glyph)
        all_bitmaps.extend(bitmap_data)
        bitmap_offset += len(bitmap_data)

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(f'#ifndef _{font_variable_name.upper()}_H_\n')
        f.write(f'#define _{font_variable_name.upper()}_H_\n\n')
        f.write('#include <Adafruit_GFX.h>\n\n')
        f.write(f'const uint8_t {font_variable_name}Bitmaps[] PROGMEM = {{\n  ')
        for i, byte in enumerate(all_bitmaps):
            f.write(f'0x{byte:02X}, ')
            if (i + 1) % 16 == 0: f.write('\n  ')
        f.write('\n};\n\n')
        all_glyphs.sort(key=lambda g: g['char_code'])
        first_char_code = all_glyphs[0]['char_code']
        last_char_code = all_glyphs[-1]['char_code']
        f.write(f'const GFXglyph {font_variable_name}Glyphs[] PROGMEM = {{\n')
        glyph_map = {g['char_code']: g for g in all_glyphs}
        for char_code in range(first_char_code, last_char_code + 1):
            if char_code in glyph_map:
                glyph = glyph_map[char_code]
                f.write(f'  {{ {glyph["bitmap_offset"]}, {glyph["width"]}, {glyph["height"]}, {glyph["x_advance"]}, {glyph["x_offset"]}, {glyph["y_offset"]} }}, // 0x{glyph["char_code"]:02X} \'{chr(glyph["char_code"])}\'\n')
            else:
                f.write(f'  {{ 0, 0, 0, 0, 0, 0 }}, // Missing char 0x{char_code:02X}\n')
        f.write('};\n\n')
        f.write(f'const GFXfont {font_variable_name} PROGMEM = {{\n')
        f.write(f'  (uint8_t  *){font_variable_name}Bitmaps,\n')
        f.write(f'  (GFXglyph *){font_variable_name}Glyphs,\n')
        f.write(f'  {first_char_code}, {last_char_code}, {font_y_advance}\n')
        f.write('};\n\n')
        f.write(f'#endif // _{font_variable_name.upper()}_H_\n')

    print(f"\n🚀 Hoàn thành! Đã tạo file '{output_file}' thành công.")
    print(f"   Tổng dung lượng bitmap: {len(all_bitmaps)} bytes")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Công cụ chuyển đổi font TTF sang định dạng Adafruit GFX.')
    # Sửa tham số --font, không bắt buộc nữa
    parser.add_argument('--font', type=str, help='Đường dẫn đến file font .ttf. Nếu bỏ trống, một cửa sổ sẽ hiện lên để chọn file.')
    parser.add_argument('--chars', type=str, required=True, help='Chuỗi ký tự cần xuất (ví dụ: "0123456789-/"')
    parser.add_argument('--size', type=int, default=24, help='Kích thước font (mặc định: 24)')
    parser.add_argument('--output', type=str, default='CustomFont.h', help='Tên file header output (mặc định: CustomFont.h)')
    parser.add_argument('--name', type=str, default='CustomFont', help='Tên biến font trong file C++ (mặc định: CustomFont)')
    
    args = parser.parse_args()

    font_path = args.font
    # **Logic mới nằm ở đây**
    if not font_path:
        print("Tham số --font không được cung cấp, mở cửa sổ chọn file...")
        font_path = get_font_path_with_gui()
        if not font_path:
            print("Không có file nào được chọn. Đã hủy.")
            exit() # Thoát script nếu người dùng không chọn file

    generate_font_header(font_path, args.size, args.chars, args.output, args.name)