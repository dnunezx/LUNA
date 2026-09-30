"""Generate a review-only CRT button concept; never modify the runtime atlas."""

from pathlib import Path
import base64
import io
import json

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/button-concepts/crt-signal"
SCALE = 8
POSITIONS = {
    "circle": (0, 0, 26, 26), "cross": (27, 0, 26, 26),
    "square": (54, 0, 26, 26), "triangle": (81, 0, 26, 26),
    "l1": (0, 28, 26, 26), "r1": (27, 28, 26, 26),
    "select": (0, 56, 22, 12), "start": (27, 56, 22, 12),
    "l2": (54, 28, 26, 26), "r2": (81, 28, 26, 26),
    "dpad": (54, 56, 26, 26), "l3": (0, 84, 26, 26),
    "r3": (27, 84, 26, 26),
}
COLORS = {
    "cross": ((77, 152, 229), (151, 208, 239)),
    "circle": ((219, 74, 96), (243, 158, 164)),
    "square": ((202, 87, 163), (236, 161, 213)),
    "triangle": ((59, 174, 128), (142, 226, 178)),
}
GLYPHS = {
    "L": ["10", "10", "10", "10", "11"],
    "R": ["110", "101", "110", "101", "101"],
    "1": ["01", "11", "01", "01", "01"],
    "2": ["111", "001", "111", "100", "111"],
    "3": ["111", "001", "111", "001", "111"],
}


def layer(base, color, mask, opacity=1.0):
    tint = Image.new("RGBA", base.size, (*color, 0))
    tint.putalpha(mask.point(lambda a: round(a * opacity)))
    base.alpha_composite(tint)


def geometry(name, width, size):
    mask = Image.new("L", (size[0] * SCALE, size[1] * SCALE))
    draw = ImageDraw.Draw(mask)

    def path(points, closed=False):
        pts = [(round(x * SCALE), round(y * SCALE)) for x, y in points]
        draw.line(pts + pts[:1] if closed else pts, fill=255,
                  width=round(width * SCALE), joint="curve")
        # Rounded joins avoid isolated bright corner pixels after rasterization.
        r = width * SCALE / 2
        for x, y in pts:
            draw.ellipse((x - r, y - r, x + r, y + r), fill=255)

    def polygon(points):
        draw.polygon([(round(x * SCALE), round(y * SCALE)) for x, y in points], fill=255)

    if name == "circle":
        # Center the stroke rather than putting its outer edge on the radius.
        r = 8 + width / 2
        draw.ellipse(tuple(round(v * SCALE) for v in (12.5-r, 12.5-r, 12.5+r, 12.5+r)),
                     outline=255, width=round(width * SCALE))
    elif name == "cross":
        path([(5.5, 5.5), (19.5, 19.5)])
        path([(19.5, 5.5), (5.5, 19.5)])
    elif name == "square":
        path([(6, 5), (19, 5), (20, 6), (20, 19), (19, 20), (6, 20), (5, 19), (5, 6)], True)
    elif name == "triangle":
        path([(12.5, 4.5), (21, 20), (4, 20)], True)
    elif name == "start":
        polygon([(5, 2), (18, 5.5), (5, 9)])
    elif name == "select":
        polygon([(3, 4), (5, 2), (17, 2), (19, 4), (19, 7), (17, 9), (5, 9), (3, 7)])
    elif name == "dpad":
        polygon([(12.5, 3), (17, 8), (8, 8)])
        polygon([(12.5, 22), (8, 17), (17, 17)])
        polygon([(3, 12.5), (8, 8), (8, 17)])
        polygon([(22, 12.5), (17, 17), (17, 8)])
    elif name in ("l3", "r3"):
        path([(8, 2), (17, 2), (23, 8), (23, 17), (17, 23), (8, 23), (2, 17), (2, 8)], True)
    else:
        # Shared silhouette, large open counterspaces, 2px bitmap lettering.
        path([(5, 4), (20, 4), (23, 7), (23, 19), (20, 22), (5, 22), (2, 19), (2, 7)], True)
    return mask.resize(size, Image.Resampling.LANCZOS)


def lettering(name):
    mask = Image.new("L", (26, 26))
    draw = ImageDraw.Draw(mask)
    pixel = 2
    total = sum(len(GLYPHS[c][0]) * pixel for c in name.upper()) + 2
    x = (26 - total) // 2
    for char in name.upper():
        glyph = GLYPHS[char]
        for row, cells in enumerate(glyph):
            for col, bit in enumerate(cells):
                if bit == "1":
                    draw.rectangle((x + col*pixel, 8 + row*pixel,
                                    x + col*pixel + 1, 9 + row*pixel), fill=255)
        x += len(glyph[0]) * pixel + 2
    return mask


def make_icon(name, size):
    image = Image.new("RGBA", size)
    edge, core = COLORS.get(name, ((90, 157, 178), (187, 213, 218)))
    mask = geometry(name, 2.8 if name in COLORS else 2.0, size)
    layer(image, edge, mask.filter(ImageFilter.GaussianBlur(0.65)), 0.16)
    layer(image, edge, mask)
    if name in COLORS:
        layer(image, core, geometry(name, 2.0, size))
    elif len(name) == 2:
        layer(image, core, lettering(name))
    else:
        layer(image, core, mask, 0.92)
    # No authored scanlines, tiny specular streaks, or animated bloom.
    return image


def data_uri(image):
    buf = io.BytesIO()
    # Controller prompts are shown on LUNA's dark canvas. Bake that viewing
    # background into preview-only images so white controls remain visible
    # when the conversation itself uses a light theme.
    preview = Image.new("RGBA", image.size, (7, 15, 26, 255))
    preview.alpha_composite(image)
    preview.save(buf, format="PNG")
    return "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    current_atlas = Image.open(ROOT / "nhddl/res/gfx/icons/icons.png").convert("RGBA")
    atlas = Image.new("RGBA", (128, 128))
    before, after = {}, {}
    for name, (x, y, w, h) in POSITIONS.items():
        before[name] = current_atlas.crop((x, y, x+w, y+h))
        after[name] = make_icon(name, (w, h))
        after[name].save(OUT / f"{name}.png")
        atlas.paste(after[name], (x, y))
    atlas.paste(current_atlas.crop((81, 56, 91, 66)), (81, 56))
    atlas.save(OUT / "icons.png")

    # A fixed, shareable comparison uses real raster assets, enlarged with nearest sampling.
    sheet = Image.new("RGB", (1120, 660), (7, 15, 26))
    draw = ImageDraw.Draw(sheet)
    font_path = ROOT / "nhddl/res/gfx/DejaVu Sans/ttf/DejaVuSans.ttf"
    title = ImageFont.truetype(str(font_path), 30)
    text = ImageFont.truetype(str(font_path), 18)
    small = ImageFont.truetype(str(font_path), 14)
    draw.text((36, 24), "LUNA / CRT SIGNAL", font=title, fill=(221, 233, 238))
    draw.text((36, 66), "Button concept   /   Native 26px controls + 22 x 12px Start and Select", font=small, fill=(137, 169, 187))
    groups = [("Face buttons", ["cross", "circle", "square", "triangle"]),
              ("Shoulders and sticks", ["l1", "r1", "l2", "r2", "l3", "r3"]),
              ("Navigation", ["dpad", "select", "start"])]
    for i, (group, names) in enumerate(groups):
        top = 112 + i * 163
        draw.text((36, top), group, font=text, fill=(221, 233, 238))
        for j, (label, icons) in enumerate((("Current", before), ("Proposed", after))):
            y = top + 40 + j * 53
            draw.text((36, y+10), label, font=small, fill=(137, 169, 187))
            for k, name in enumerate(names):
                icon = icons[name]
                pos = (180 + k * 145, y + (26 - icon.height))
                enlarged = icon.resize((icon.width*2, icon.height*2), Image.Resampling.NEAREST)
                sheet.paste(enlarged, pos, enlarged)
                draw.text((pos[0]+64, y+17), name.upper(), font=small, fill=(177, 197, 208))
    draw.text((36, 625), "2x raster inspection   /   Restrained halo, bright colored cores, 2px lettering   /   Concept only; hardware check pending", font=small, fill=(137, 169, 187))
    sheet.save(OUT / "comparison.png")

    # Populate a literal HTML fragment template; no external resource access needed.
    template = (ROOT / "tools/crt-button-review.html").read_text(encoding="utf-8")
    payload = {"before": {n: data_uri(im) for n, im in before.items()},
               "after": {n: data_uri(im) for n, im in after.items()},
               "sizes": {n: [p[2], p[3]] for n, p in POSITIONS.items()}}
    fragment = template.replace("/*ASSET_DATA*/{}", json.dumps(payload, separators=(",", ":")))
    visual = Path("C:/Users/ddx/.codex/visualizations/2026/09/30/01a0f23c-2838-7483-949f-a9ca6baeb602/crt-button-review.html")
    visual.write_text(fragment, encoding="utf-8")
    print(f"Created {len(after)} concept icons, compatible atlas, comparison, and review preview.")


if __name__ == "__main__":
    main()
