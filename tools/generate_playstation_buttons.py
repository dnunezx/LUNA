"""Generate a centered, matching set of 26px PlayStation button icons."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


SIZE = 26
SCALE = 12
CENTER = 12.5
OUTPUT = Path(__file__).resolve().parents[1] / "assets" / "playstation-buttons"


def stroke_mask(symbol: str, width: float) -> Image.Image:
    mask = Image.new("L", (SIZE * SCALE, SIZE * SCALE))
    draw = ImageDraw.Draw(mask)
    line_width = round(width * SCALE)
    # Align the visible stroke mass to the center pixel between 12 and 13.
    shift_x = 0.5
    shift_y = 0.5

    def path(points: tuple[tuple[float, float], ...], closed: bool = False) -> None:
        vertices = points + (points[0],) if closed else points
        draw.line([(round((x + shift_x) * SCALE), round((y + shift_y) * SCALE)) for x, y in vertices], fill=255, width=line_width, joint="curve")
        radius = width * SCALE / 2
        for x, y in points:
            cx, cy = (x + shift_x) * SCALE, (y + shift_y) * SCALE
            draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill=255)

    if symbol == "circle":
        outer_radius = 7.5
        box = tuple(round(v * SCALE) for v in (CENTER - outer_radius + shift_x, CENTER - outer_radius + shift_y, CENTER + outer_radius + shift_x, CENTER + outer_radius + shift_y))
        draw.ellipse(box, outline=255, width=line_width)
    elif symbol == "square":
        path(((6.5, 6.5), (18.5, 6.5), (18.5, 18.5), (6.5, 18.5)), closed=True)
    elif symbol == "triangle":
        path(((CENTER, 6.5), (19.4, 18.5), (5.6, 18.5)), closed=True)
    elif symbol == "cross":
        path(((6.8, 6.8), (18.2, 18.2)))
        path(((18.2, 6.8), (6.8, 18.2)))
    else:
        raise ValueError(f"Unknown symbol: {symbol}")
    return mask.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def layer(base: Image.Image, color: tuple[int, int, int], mask: Image.Image, opacity: float) -> None:
    alpha = mask.point(lambda value: round(value * opacity))
    tint = Image.new("RGBA", (SIZE, SIZE), color + (0,))
    tint.putalpha(alpha)
    base.alpha_composite(tint)


def make_icon(symbol: str, color: tuple[int, int, int]) -> Image.Image:
    image = Image.new("RGBA", (SIZE, SIZE))
    broad = stroke_mask(symbol, 3.0)
    core = stroke_mask(symbol, 2.1)
    thin = stroke_mask(symbol, 0.9)

    layer(image, color, broad.filter(ImageFilter.GaussianBlur(2.4)), 0.20)
    layer(image, color, broad.filter(ImageFilter.GaussianBlur(1.1)), 0.34)
    layer(image, color, broad, 0.25)
    layer(image, color, core, 0.68)

    # Bright upper-left lighting follows the magenta square and green triangle.
    highlight = Image.new("L", (SIZE, SIZE))
    high_pixels = highlight.load()
    thin_pixels = thin.load()
    for y in range(SIZE):
        for x in range(SIZE):
            strength = max(0.0, 1.0 - 0.025 * x - 0.024 * y)
            high_pixels[x, y] = round(thin_pixels[x, y] * strength * 0.49)
    layer(image, (255, 255, 255), highlight, 1.0)
    return image


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for symbol, color in (
        ("cross", (35, 113, 210)),
        ("circle", (214, 43, 68)),
        ("triangle", (20, 143, 59)),
        ("square", (202, 27, 124)),
    ):
        make_icon(symbol, color).save(OUTPUT / f"{symbol}.png")


if __name__ == "__main__":
    main()
