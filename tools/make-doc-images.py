"""Package native-rendered documentation images. Requires Pillow.

No desktop screenshots or live session data are read. The only screenshot is
the user-supplied actual-tray.png already copied into docs/images.
"""
import json
import struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
OUT = ROOT / "docs" / "images"
OUT.mkdir(parents=True, exist_ok=True)
BACKGROUND = (217, 217, 217)
LABELS = ["Idle", "Working", "Completed", "Question", "Error", "Stopped",
          "Detection uncertain", "Usage limit", "Long task (10+ min)",
          "Working + 3 unread"]
FONT = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 18)
TITLE_FONT = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 22)


def icon(name):
    data = (BUILD / name).read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    return Image.frombytes("RGBA", (32, 32), data[offset:offset + 4096],
                           "raw", "BGRA")


def enlarged(image, size=128):
    result = Image.new("RGB", (size, size), BACKGROUND)
    source = image.resize((size, size), Image.Resampling.NEAREST)
    result.paste(source, (0, 0), source)
    return result


gallery_frames = []
for frame in range(4):
    canvas = Image.new("RGB", (1000, 400), BACKGROUND)
    draw = ImageDraw.Draw(canvas)
    for state, label in enumerate(LABELS):
        image = icon(f"icon-{state}-{frame}.bmp" if state < 9
                     else f"badge-{frame}.bmp")
        left, top = (state % 5) * 200, (state // 5) * 200
        canvas.paste(enlarged(image), (left + 36, top + 12))
        box = draw.textbbox((0, 0), label, font=FONT)
        draw.text((left + (200 - box[2]) // 2, top + 157), label,
                  font=FONT, fill=(28, 31, 36))
    gallery_frames.append(canvas)
gallery_frames[0].save(OUT / "states.png", optimize=True)
gallery_frames[0].save(OUT / "states.gif", save_all=True,
                       append_images=gallery_frames[1:], duration=300,
                       loop=0, disposal=2, optimize=False)
for state, filename in [(1, "working.gif"), (7, "usage-limit.gif")]:
    frames = [enlarged(icon(f"icon-{state}-{frame}.bmp"), 256)
              for frame in range(4)]
    frames[0].save(OUT / filename, save_all=True, append_images=frames[1:],
                   duration=300, loop=0, disposal=2, optimize=False)

panel_labels = ["Running: elapsed time", "Completed: unread dots",
                "Question: elapsed time + ?", "Error: unread result dots",
                "Usage limit: resume markers"]
panels = []
for number in range(5):
    image = Image.open(BUILD / f"panel-{number}.bmp").convert("RGB")
    image.save(OUT / f"panel-{number}.png", optimize=True)
    panels.append(image.resize((560, image.height * 2), Image.Resampling.LANCZOS))
panel_gallery = Image.new("RGB", (1200, 780), (239, 239, 239))
draw = ImageDraw.Draw(panel_gallery)
for number, image in enumerate(panels):
    left, top = 20 + (number % 2) * 600, 12 + (number // 2) * 260
    draw.text((left, top), panel_labels[number], font=TITLE_FONT, fill=(28, 31, 36))
    panel_gallery.paste(image, (left, top + 40))
panel_gallery.save(OUT / "session-examples.png", optimize=True)

report = []
for file in sorted(OUT.iterdir()):
    if file.suffix not in (".png", ".gif"):
        continue
    with Image.open(file) as image:
        frames = getattr(image, "n_frames", 1)
        variants = set()
        durations = []
        for frame in range(frames):
            image.seek(frame)
            variants.add(image.convert("RGB").tobytes())
            durations.append(image.info.get("duration"))
        if file.suffix == ".gif":
            assert frames >= 2 and len(variants) >= 2, file
            assert image.info.get("loop") == 0, file
            assert all(duration == 300 for duration in durations), file
        report.append(dict(file=file.name, size=list(image.size), frames=frames,
                           distinct_frames=len(variants), bytes=file.stat().st_size))
print(json.dumps(report, indent=2))
