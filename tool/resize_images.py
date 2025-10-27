import argparse
import os
from pathlib import Path
from PIL import Image, ImageOps

def hex_to_rgb(hexstr: str):
    hexstr = hexstr.strip().lstrip('#')
    if len(hexstr) == 3:
        hexstr = ''.join([c*2 for c in hexstr])
    if len(hexstr) != 6:
        raise ValueError("Mã màu HEX không hợp lệ, ví dụ: #000000 hoặc #fff")
    r = int(hexstr[0:2], 16)
    g = int(hexstr[2:4], 16)
    b = int(hexstr[4:6], 16)
    return (r, g, b)

def is_image_file(p: Path):
    return p.suffix.lower() in {".jpg", ".jpeg", ".png", ".bmp", ".gif", ".webp"}

def load_image_with_orientation(p: Path) -> Image.Image:
    # Mở ảnh và tôn trọng EXIF Orientation nếu có
    im = Image.open(p)
    try:
        im = ImageOps.exif_transpose(im)
    except Exception:
        pass
    return im

def resize_fit(img: Image.Image, target_w: int, target_h: int, bg_rgb=(0,0,0)) -> Image.Image:
    # Giữ tỉ lệ, chèn nền để vừa khít khung
    img_copy = img.copy()
    img_copy.thumbnail((target_w, target_h), Image.LANCZOS)
    canvas = Image.new("RGB", (target_w, target_h), bg_rgb)
    x = (target_w - img_copy.width) // 2
    y = (target_h - img_copy.height) // 2
    canvas.paste(img_copy, (x, y))
    return canvas

def resize_fill(img: Image.Image, target_w: int, target_h: int) -> Image.Image:
    # Giữ tỉ lệ, cắt thừa
    src_w, src_h = img.size
    src_ratio = src_w / src_h
    tgt_ratio = target_w / target_h

    if src_ratio > tgt_ratio:
        # ảnh rộng hơn → scale theo chiều cao, cắt ngang
        new_h = target_h
        new_w = int(src_ratio * new_h)
    else:
        # ảnh cao hơn → scale theo chiều rộng, cắt dọc
        new_w = target_w
        new_h = int(new_w / src_ratio)

    img_resized = img.resize((new_w, new_h), Image.LANCZOS)
    # cắt giữa
    left = (new_w - target_w) // 2
    top = (new_h - target_h) // 2
    right = left + target_w
    bottom = top + target_h
    return img_resized.crop((left, top, right, bottom))

def resize_stretch(img: Image.Image, target_w: int, target_h: int) -> Image.Image:
    # Không giữ tỉ lệ
    return img.resize((target_w, target_h), Image.LANCZOS)

def main():
    parser = argparse.ArgumentParser(description="Resize ảnh hàng loạt từ một thư mục.")
    parser.add_argument("-i", "--input", required=True, help="Thư mục chứa ảnh nguồn")
    parser.add_argument("-o", "--output", required=True, help="Thư mục xuất ảnh")
    parser.add_argument("-W", "--width", type=int, required=True, help="Chiều rộng đích (px)")
    parser.add_argument("-H", "--height", type=int, required=True, help="Chiều cao đích (px)")
    parser.add_argument("-m", "--mode", default="fit", choices=["fit", "fill", "stretch"],
                        help="Chế độ resize: fit (giữ tỉ lệ + nền), fill (giữ tỉ lệ + cắt), stretch (kéo giãn)")
    parser.add_argument("--bg", default="#000000", help="Màu nền khi dùng mode fit (HEX, ví dụ #000000)")
    parser.add_argument("--quality", type=int, default=90, help="JPEG quality (0-100)")
    parser.add_argument("--prefix", default="", help="Tiền tố tên file xuất (vd: ss_)")
    parser.add_argument("--suffix", default="", help="Hậu tố tên file xuất (vd: _240)")
    parser.add_argument("--format", default="jpg", choices=["jpg","jpeg","png","webp","bmp"],
                        help="Định dạng xuất")
    parser.add_argument("--recursive", action="store_true", help="Duyệt đệ quy thư mục con")
    args = parser.parse_args()

    in_dir = Path(args.input)
    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    bg_rgb = hex_to_rgb(args.bg)
    save_ext = "." + ("jpg" if args.format == "jpeg" else args.format)

    files = []
    if args.recursive:
        files = [p for p in in_dir.rglob("*") if p.is_file() and is_image_file(p)]
    else:
        files = [p for p in in_dir.iterdir() if p.is_file() and is_image_file(p)]

    if not files:
        print("⚠️ Không tìm thấy ảnh trong thư mục đầu vào.")
        return

    print(f"🔧 Chế độ: {args.mode} | Kích thước đích: {args.width}x{args.height} | Ảnh: {len(files)} tệp")

    for idx, p in enumerate(files, 1):
        try:
            img = load_image_with_orientation(p).convert("RGB")
            if args.mode == "fit":
                out_img = resize_fit(img, args.width, args.height, bg_rgb)
            elif args.mode == "fill":
                out_img = resize_fill(img, args.width, args.height)
            else:
                out_img = resize_stretch(img, args.width, args.height)

            # Tạo tên file xuất
            rel = p.relative_to(in_dir) if args.recursive else p.name
            rel_name = rel.stem if isinstance(rel, Path) else Path(rel).stem
            out_name = f"{args.prefix}{rel_name}{args.suffix}{save_ext}"

            # Nếu duyệt đệ quy: giữ cây thư mục
            out_path = out_dir / (rel.parent if isinstance(rel, Path) else Path()) / out_name
            out_path.parent.mkdir(parents=True, exist_ok=True)

            # Lưu
            save_kwargs = {}
            if save_ext.lower() in [".jpg", ".jpeg", ".webp"]:
                save_kwargs["quality"] = args.quality
                if save_ext.lower() in [".jpg", ".jpeg"]:
                    save_kwargs["optimize"] = True
                    save_kwargs["progressive"] = True
            out_img.save(out_path, **save_kwargs)
            print(f"[{idx}/{len(files)}] ✅ {p}  →  {out_path}")
        except Exception as e:
            print(f"[{idx}/{len(files)}] ❌ {p}  →  lỗi: {e}")

    print("🎉 Xong!")

if __name__ == "__main__":
    main()
