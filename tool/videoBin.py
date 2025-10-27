import os
import struct
import glob

# --- CẤU HÌNH ---
# Thư mục chứa các frame ảnh JPEG của bạn (ví dụ: frame_001.jpg, frame_002.jpg, ...)
INPUT_FOLDER = "frames" 
# Tên file .bin sẽ được tạo ra
OUTPUT_FILE = "video_custom.bin"

def create_video_bundle():
    """
    Tìm tất cả các file .jpg trong thư mục, sắp xếp chúng,
    và đóng gói thành một file .bin duy nhất.
    """
    print(f"Bắt đầu quá trình đóng gói từ thư mục: '{INPUT_FOLDER}'")

    # 1. Tìm và sắp xếp tất cả các file frame
    # Sử dụng glob để tìm tất cả file .jpg và .jpeg
    frame_files = sorted(glob.glob(os.path.join(INPUT_FOLDER, '*.jpg')) + glob.glob(os.path.join(INPUT_FOLDER, '*.jpeg')))

    if not frame_files:
        print(f"Lỗi: Không tìm thấy file .jpg hoặc .jpeg nào trong thư mục '{INPUT_FOLDER}'.")
        return

    num_frames = len(frame_files)
    print(f"Tìm thấy {num_frames} frame ảnh.")

    # 2. Mở file output ở chế độ ghi nhị phân (binary write)
    with open(OUTPUT_FILE, 'wb') as f:
        # --- GHI HEADER VÀ BẢNG CHỈ MỤC TẠM THỜI ---

        # Ghi số lượng frame (4 bytes, little-endian)
        f.write(struct.pack('<I', num_frames))
        
        # Tạo không gian trống cho bảng chỉ mục (Index Table)
        # Mỗi entry trong bảng chỉ mục gồm: offset (4 bytes) + size (4 bytes) = 8 bytes
        index_table_size = num_frames * 8
        f.write(b'\x00' * index_table_size)

        # --- GHI DỮ LIỆU CÁC FRAME ---
        
        frame_metadata = []
        current_offset = f.tell() # Lấy vị trí bắt đầu của dữ liệu frame đầu tiên

        print("Đang ghi dữ liệu các frame...")
        for frame_path in frame_files:
            with open(frame_path, 'rb') as frame_file:
                frame_data = frame_file.read()
                frame_size = len(frame_data)
                
                # Ghi dữ liệu frame vào file .bin
                f.write(frame_data)
                
                # Lưu lại thông tin offset và size để ghi vào bảng chỉ mục sau
                frame_metadata.append({'offset': current_offset, 'size': frame_size})
                
                # Cập nhật offset cho frame tiếp theo
                current_offset += frame_size
        
        print("Hoàn tất ghi dữ liệu frame.")

        # --- CẬP NHẬT LẠI BẢNG CHỈ MỤC ---

        print("Đang cập nhật lại bảng chỉ mục...")
        # Di chuyển con trỏ file về vị trí ngay sau header (vị trí bắt đầu của bảng chỉ mục)
        f.seek(4) 
        
        for metadata in frame_metadata:
            # Ghi offset (4 bytes) và size (4 bytes) cho từng frame
            f.write(struct.pack('<I', metadata['offset']))
            f.write(struct.pack('<I', metadata['size']))
            
        print("Hoàn tất!")
        print(f"Tạo file '{OUTPUT_FILE}' thành công.")


if __name__ == "__main__":
    # Tạo thư mục input nếu chưa có để người dùng tiện sử dụng
    if not os.path.exists(INPUT_FOLDER):
        os.makedirs(INPUT_FOLDER)
        print(f"Đã tạo thư mục '{INPUT_FOLDER}'. Hãy sao chép các frame ảnh của bạn vào đây.")
    else:
        create_video_bundle()
