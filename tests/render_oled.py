"""按 Adafruit 经典位图字体生成 128x64 布局预览（仅显示示例数据）。"""
import argparse
import re
from pathlib import Path
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument('font', type=Path, help='Adafruit_GFX_Library/glcdfont.c')
parser.add_argument('output', type=Path)
args = parser.parse_args()
source = args.font.read_text(encoding='utf-8')
array = source.split('font[] PROGMEM = {', 1)[1].split('};', 1)[0]
font = [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]{2})', array)]

def render(cpu, gpu, duty):
    canvas = Image.new('RGB', (128, 64), 'black')
    draw = ImageDraw.Draw(canvas)
    def text(value, x, y, size=1):
        for char in str(value):
            assert 0 <= x and x + 5 * size <= 128 and 0 <= y and y + 7 * size <= 64
            for col in range(5):
                bits = font[ord(char) * 5 + col]
                for row in range(8):
                    if bits & (1 << row):
                        draw.rectangle((x + col*size, y + row*size,
                                        x + (col+1)*size-1, y + (row+1)*size-1), fill='white')
            x += 6 * size
    for label, value, x in [('CPU', cpu, 0), ('GPU', gpu, 64)]:
        text(label + ' C', x + 17, 0)
        value = '--' if value is None else str(int(value + 0.5))
        size = 5 if len(value) <= 2 else 3
        text(value, x + (64-len(value)*6*size)//2, 10 + (40-8*size)//2, size)
    draw.line((2, 50, 125, 50), fill='white')
    text('CON', 2, 55)
    speed = f'SPEED {duty}%'
    text(speed, (128-len(speed)*6)//2, 55)
    text('QUIET', 96, 55)
    return canvas

args.output.parent.mkdir(parents=True, exist_ok=True)
for suffix, values in [('', (70, 68, 40)), ('-max', (120, 100, 100)), ('-missing', (None, 60, 23))]:
    screen = render(*values)
    screen.resize((1024, 512), Image.Resampling.NEAREST).save(args.output.with_name(args.output.stem + suffix + '.png'))
print('已生成正常、三位数和缺失温度预览，像素边界检查通过。')
