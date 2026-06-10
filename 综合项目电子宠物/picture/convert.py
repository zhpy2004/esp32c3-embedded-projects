"""
将 6 张裁剪好的宠物 PNG 转换为 SSD1680 电子墨水屏 C 数组
等比缩放居中, 不拉伸变形
输出: pet_images.h (122×250, BW + RED 双层, 每层 4000 字节)
"""

from PIL import Image
import numpy as np
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "..", "pet_game")

EPD_WIDTH = 122
EPD_HEIGHT = 250
EPD_W_BYTES = 16
EPD_BUF_SIZE = EPD_W_BYTES * EPD_HEIGHT  # 4000

IMAGES = [
    ("开心.png",  "HAPPY"),
    ("普通.png",  "NORMAL"),
    ("饥饿.png",  "HUNGRY"),
    ("难过.png",  "SAD"),
    ("睡觉.png",  "SLEEP"),
    ("死亡.png",  "DEAD"),
]


def fit_to_canvas(img, canvas_w, canvas_h):
    """等比缩放图片, 居中放在白色画布上"""
    w, h = img.size
    scale = min(canvas_w / w, canvas_h / h)
    new_w = int(w * scale)
    new_h = int(h * scale)
    resized = img.resize((new_w, new_h), Image.NEAREST)

    canvas = Image.new("RGB", (canvas_w, canvas_h), (255, 255, 255))
    x_off = (canvas_w - new_w) // 2
    y_off = (canvas_h - new_h) // 2
    canvas.paste(resized, (x_off, y_off))
    return canvas


def classify_pixels(arr):
    """将 RGB 像素分类为 白/黑/红, 灰色(亮度80-200)归为黑色"""
    r = arr[:, :, 0].astype(int)
    g = arr[:, :, 1].astype(int)
    b = arr[:, :, 2].astype(int)
    brightness = (r + g + b) / 3

    is_red = (r > 150) & (g < 100) & (b < 100)
    is_white = (~is_red) & (brightness >= 200)

    bw_map = is_white       # bit=1 白, bit=0 黑或红
    red_map = is_red         # bit=1 红
    return bw_map, red_map


def bitmap_to_bytes(pixel_map):
    """布尔像素图 (122×250) → 字节数组, 每行16字节, MSB在前"""
    data = bytearray(EPD_BUF_SIZE)
    h, w = pixel_map.shape
    for y in range(h):
        for x in range(w):
            if pixel_map[y, x]:
                byte_idx = y * EPD_W_BYTES + (x // 8)
                bit_idx = 7 - (x % 8)
                data[byte_idx] |= (1 << bit_idx)
    return bytes(data)


def bytes_to_c_array(data, name):
    """字节数据 → C 数组字符串"""
    lines = [f"static const uint8_t {name}[EPD_BUF_SIZE] PROGMEM = {{"]
    for row in range(EPD_HEIGHT):
        offset = row * EPD_W_BYTES
        row_bytes = data[offset:offset + EPD_W_BYTES]
        hex_str = ", ".join(f"0x{b:02X}" for b in row_bytes)
        comma = "," if row < EPD_HEIGHT - 1 else ""
        lines.append(f"    {hex_str}{comma}")
    lines.append("};")
    return "\n".join(lines)


def convert_image(filepath, name):
    img = Image.open(filepath).convert("RGB")
    print(f"  原始尺寸: {img.size}")

    canvas = fit_to_canvas(img, EPD_WIDTH, EPD_HEIGHT)
    arr = np.array(canvas)
    bw_map, red_map = classify_pixels(arr)

    white_count = int(np.sum(bw_map))
    black_count = int(np.sum(~bw_map & ~red_map))
    red_count = int(np.sum(red_map))
    print(f"  像素统计: 白={white_count}, 黑={black_count}, 红={red_count}")

    bw_bytes = bitmap_to_bytes(bw_map)
    red_bytes = bitmap_to_bytes(red_map)

    # 保存预览
    preview_arr = np.full((EPD_HEIGHT, EPD_WIDTH, 3), 255, dtype=np.uint8)
    preview_arr[~bw_map & ~red_map] = [0, 0, 0]
    preview_arr[red_map] = [255, 0, 0]
    Image.fromarray(preview_arr).save(
        os.path.join(SCRIPT_DIR, f"preview_{name}.png"))

    bw_c = bytes_to_c_array(bw_bytes, f"IMG_{name}_BW")
    red_c = bytes_to_c_array(red_bytes, f"IMG_{name}_RED")
    return bw_c, red_c


def generate_header(image_arrays):
    lines = [
        '/**',
        ' * @file    pet_images.h',
        ' * @brief   宠物外观位图数据 (PROGMEM) — 6 种外观 × BW/RED 双层',
        ' *',
        ' * @note    图像格式: 122×250, 每行 16 字节 (128 像素, 仅用 122), 共 250 行',
        ' *          BW 层: bit=1 白, bit=0 黑 | RED 层: bit=1 红, bit=0 非红',
        ' *          每张图 4000 字节',
        ' *',
        ' * @warning 本文件由 convert.py 自动生成',
        ' */',
        '',
        '#ifndef PET_IMAGES_H',
        '#define PET_IMAGES_H',
        '',
        '#include <Arduino.h>',
        '#include "epd_ssd1680.h"',
        '#include "game_config.h"',
        '',
    ]

    for name, (bw_c, red_c) in image_arrays.items():
        lines.append(f"/* ---- {name} ---- */")
        lines.append(bw_c)
        lines.append("")
        lines.append(red_c)
        lines.append("")

    lines.append("/* BW 层查找表 */")
    lines.append("static const uint8_t * const PET_IMG_BW[APPEAR_COUNT] = {")
    for _, name in IMAGES:
        lines.append(f"    IMG_{name}_BW,")
    lines.append("};")
    lines.append("")
    lines.append("/* RED 层查找表 */")
    lines.append("static const uint8_t * const PET_IMG_RED[APPEAR_COUNT] = {")
    for _, name in IMAGES:
        lines.append(f"    IMG_{name}_RED,")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* PET_IMAGES_H */")

    return "\n".join(lines)


def main():
    print("=" * 50)
    print("电子宠物位图转换")
    print(f"目标: {EPD_WIDTH}x{EPD_HEIGHT}, {EPD_BUF_SIZE} bytes/layer")
    print("=" * 50)

    image_arrays = {}
    for filename, name in IMAGES:
        filepath = os.path.join(SCRIPT_DIR, filename)
        print(f"\n[{name}] {filename}")
        if not os.path.exists(filepath):
            print(f"  *** 文件不存在, 跳过 ***")
            continue
        bw_c, red_c = convert_image(filepath, name)
        image_arrays[name] = (bw_c, red_c)

    if len(image_arrays) == 6:
        header = generate_header(image_arrays)
        output_path = os.path.join(OUTPUT_DIR, "pet_images.h")
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(header)
        print(f"\n已生成: {output_path}")
        print(f"文件大小: {os.path.getsize(output_path)} bytes")
    else:
        print(f"\n只找到 {len(image_arrays)}/6 张图, 未生成头文件")


if __name__ == "__main__":
    main()
