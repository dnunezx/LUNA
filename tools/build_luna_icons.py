"""Build LUNA's button atlas and the embedded copy used by the PS2 UI."""

from pathlib import Path
import re

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ATLAS = ROOT / "nhddl/res/gfx/icons/icons.png"
HEADER = ROOT / "nhddl/include/ui/icons.h"
SIZE = 26
KENNEY = ROOT / "assets/kenney-input-prompts"
LUNA_PROMPTS = ROOT / "assets/luna-prompts"


def make_textless_button(name: str) -> Image.Image:
    """Draw Luna's wordless Select rectangle or Start arrow at icon size."""
    scale = 4
    white = (255, 255, 255, 255)
    icon = Image.new("RGBA", (SIZE * scale, SIZE * scale))
    draw = ImageDraw.Draw(icon)
    if name == "select":
        draw.rounded_rectangle((3 * scale, 8 * scale, 22 * scale, 18 * scale),
                               radius=2 * scale, outline=white, width=2 * scale)
    elif name == "start":
        points = [(4 * scale, 7 * scale), (22 * scale, 13 * scale),
                  (4 * scale, 19 * scale)]
        draw.line(points + points[:1], fill=white, width=2 * scale, joint="curve")
        for x, y in points:
            draw.ellipse((x - scale, y - scale, x + scale, y + scale), fill=white)
    else:
        raise ValueError(name)
    return icon.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def main() -> None:
    atlas = Image.new("RGBA", (128, 128))
    for name, x in (("circle", 0), ("cross", 27), ("square", 54), ("triangle", 81)):
        icon = Image.open(ROOT / f"assets/playstation-buttons/{name}.png").convert("RGBA")
        atlas.paste(icon, (x, 0))
    for name, x, y in (
        ("playstation_trigger_l1_outline", 0, 28),
        ("playstation_trigger_r1_outline", 27, 28),
        ("playstation_trigger_l2_outline", 54, 28),
        ("playstation_trigger_r2_outline", 81, 28),
        ("playstation_dpad", 54, 56),
        ("playstation_button_l3_outline", 0, 84),
        ("playstation_button_r3_outline", 27, 84),
    ):
        icon = Image.open(KENNEY / f"{name}.png").convert("RGBA")
        atlas.paste(icon.resize((SIZE, SIZE), Image.Resampling.LANCZOS), (x, y))
    LUNA_PROMPTS.mkdir(exist_ok=True)
    for name, x in (("select", 0), ("start", 27)):
        icon = make_textless_button(name)
        icon.save(LUNA_PROMPTS / f"{name}.png")
        atlas.paste(icon, (x, 56))
    ImageDraw.Draw(atlas).ellipse((81, 56, 90, 65), fill=(0, 184, 209, 255))
    atlas.save(ATLAS)

    data = ATLAS.read_bytes()
    original = HEADER.read_bytes()
    eol = b"\r\n" if b"\r\n" in original else b"\n"
    rows = [b"    " + b", ".join(b"0x%02x" % byte for byte in data[i:i + 16]) + b","
            for i in range(0, len(data), 16)]
    replacement = (b"unsigned int SIZE_ICONS_PNG = " + str(len(data)).encode() + b";" + eol +
                   b"unsigned char ICONS_PNG[] __attribute__((aligned(16))) = {" + eol +
                   eol.join(rows) + eol + b"};")
    pattern = (rb"unsigned int SIZE_ICONS_PNG = \d+;\r?\n"
               rb"unsigned char ICONS_PNG\[\] __attribute__\(\(aligned\(16\)\)\) = \{\r?\n"
               rb".*?\r?\n\};")
    updated, count = re.subn(pattern, lambda _: replacement, original, flags=re.S)
    if count != 1:
        raise RuntimeError("Could not find the embedded icon PNG in icons.h")
    HEADER.write_bytes(updated)


if __name__ == "__main__":
    main()
