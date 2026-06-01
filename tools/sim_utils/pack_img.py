import cv2
import os
import sys

def pack_img(img_path, out_dir=None):
    # 1. 读取灰度图
    img = cv2.imread(img_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        print(f"ERROR: Cannot read image: {img_path}")
        return

    H = img.shape[1]
    V = img.shape[0]

    # 2. 打包为 32-bit 字: 4 像素/字, LSB=最左像素
    #    beat[7:0]=pix[col+0], [15:8]=pix[col+1], [23:16]=pix[col+2], [31:24]=pix[col+3]
    HB = (H + 3) // 4  # beats per row (rounded up)
    words = []
    for row in range(V):
        for col in range(0, HB * 4, 4):
            # zero-pad beyond image width
            b0 = int(img[row, col + 0]) if col + 0 < H else 0
            b1 = int(img[row, col + 1]) if col + 1 < H else 0
            b2 = int(img[row, col + 2]) if col + 2 < H else 0
            b3 = int(img[row, col + 3]) if col + 3 < H else 0
            word = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0
            words.append(word)

    # 3. determine output directory
    if out_dir is None:
        out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "sim_logger")
    os.makedirs(out_dir, exist_ok=True)

    # 4. write input.hex (one 32-bit hex per line, for $readmemh)
    hex_path = os.path.join(out_dir, "input.hex")
    with open(hex_path, "w") as f:
        for w in words:
            f.write(f"{w:08X}\n")

    # 5. write input_cfg.txt (H_ACTIVE V_ACTIVE on one line)
    cfg_path = os.path.join(out_dir, "input_cfg.txt")
    with open(cfg_path, "w") as f:
        f.write(f"{H} {V}\n")

    print(f"Image : {H}x{V} ({HB} beats/row, {len(words)} words total)")
    print(f"Hex   : {hex_path}")
    print(f"Config: {cfg_path}")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Pack grayscale image into input.hex + cfg for svtb")
    parser.add_argument("image", help="Path to input image (any size, grayscale)")
    parser.add_argument("--outdir", "-o", default=None, help="Output directory for input.hex / input_cfg.txt")
    args = parser.parse_args()
    pack_img(args.image, args.outdir)
