import argparse, os, struct
from pathlib import Path
from PIL import Image, ImageOps

def hex_to_rgb(hexstr):
    hexstr = hexstr.strip().lstrip('#')
    if len(hexstr) == 3: hexstr = ''.join([c*2 for c in hexstr])
    return tuple(int(hexstr[i:i+2], 16) for i in (0,2,4))

def is_image_file(p: Path):
    return p.suffix.lower() in {".jpg", ".jpeg", ".png", ".bmp", ".gif", ".webp"}

def load_image_with_orientation(p: Path):
    im = Image.open(p)
    try: im = ImageOps.exif_transpose(im)
    except Exception: pass
    return im

def resize_fit(img, w, h, bg=(0,0,0)):
    img = img.copy(); img.thumbnail((w,h), Image.LANCZOS)
    canvas = Image.new("RGB", (w,h), bg)
    x = (w - img.width)//2; y = (h - img.height)//2
    canvas.paste(img, (x,y)); return canvas

def resize_fill(img, w, h):
    src_w, src_h = img.size; src_r = src_w/src_h; tgt_r = w/h
    if src_r > tgt_r:
        new_h = h; new_w = int(src_r * new_h)
    else:
        new_w = w; new_h = int(new_w / src_r)
    img = img.resize((new_w,new_h), Image.LANCZOS)
    left = (new_w - w)//2; top = (new_h - h)//2
    return img.crop((left, top, left+w, top+h))

def resize_stretch(img, w, h):
    return img.resize((w,h), Image.LANCZOS)

def main():
    parser = argparse.ArgumentParser(description="Resize ảnh và tùy chọn đóng gói thành .bin cho ESP32 video.")
    parser.add_argument("-i","--input",required=True)
    parser.add_argument("-o","--output",required=True)
    parser.add_argument("-W","--width",type=int,required=True)
    parser.add_argument("-H","--height",type=int,required=True)
    parser.add_argument("-m","--mode",default="fit",choices=["fit","fill","stretch"])
    parser.add_argument("--bg",default="#000000")
    parser.add_argument("--quality",type=int,default=90)
    parser.add_argument("--pack-bin",action="store_true",help="Đóng gói tất cả ảnh thành video_custom.bin")
    parser.add_argument("--prefix",default="",help="Tiền tố ảnh khi không pack bin")
    parser.add_argument("--format",default="jpg",choices=["jpg","jpeg","png","webp"])
    args = parser.parse_args()

    in_dir, out_dir = Path(args.input), Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)
    bg = hex_to_rgb(args.bg)
    files = sorted([p for p in in_dir.iterdir() if is_image_file(p)])
    if not files: print("⚠️ Không tìm thấy ảnh"); return

    print(f"🔧 {len(files)} ảnh, mode={args.mode}, kích thước={args.width}x{args.height}")
    frames = []

    for idx,p in enumerate(files,1):
        img = load_image_with_orientation(p).convert("RGB")
        if args.mode=="fit": out = resize_fit(img,args.width,args.height,bg)
        elif args.mode=="fill": out = resize_fill(img,args.width,args.height)
        else: out = resize_stretch(img,args.width,args.height)
        out_path = out_dir / f"{args.prefix}{p.stem}.{args.format}"
        out.save(out_path, quality=args.quality)
        print(f"[{idx}/{len(files)}] ✅ {p.name} -> {out_path.name}")
        frames.append(out_path)

    if not args.pack_bin:
        print("🎉 Xong! (Không đóng gói)")
        return

    # --- pack to bin ---
    bin_path = out_dir / "video_custom.bin"
    with open(bin_path,"wb") as fbin:
        num_frames = len(frames)
        print(f"📦 Đang đóng gói {num_frames} frame thành {bin_path.name}")
        fbin.write(struct.pack("<I", num_frames))
        index_offset = fbin.tell() + num_frames*8
        offsets = []
        cur = index_offset
        for frame in frames:
            data = frame.read_bytes()
            offsets.append((cur,len(data)))
            cur += len(data)
        # ghi bảng chỉ mục
        for off,size in offsets:
            fbin.write(struct.pack("<II", off, size))
        # ghi dữ liệu
        for frame in frames:
            fbin.write(frame.read_bytes())
    print(f"✅ Đã tạo {bin_path} ({bin_path.stat().st_size/1024:.1f} KB)")

if __name__=="__main__":
    main()

# --- KẾT THÚC FILE --- #

# python resize_images.py -i "D:\input" -o "D:\out" -W 240 -H 240 -m fit --pack-bin --bg #000000
# python resize_images.py -i "D:\input" -o "D:\out" -W 240 -H 240 -m fill
