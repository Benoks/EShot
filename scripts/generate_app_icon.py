from pathlib import Path

from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "icons" / "app_base.ico"
OUTPUT = ROOT / "app.ico"
SIZES = (16, 24, 32, 48, 64, 128, 256)


def outlined_icon(source: Image.Image, size: int) -> Image.Image:
    padding = max(1, round(size * 0.08))
    content_size = size - (padding * 2)
    icon = source.resize((content_size, content_size), Image.Resampling.LANCZOS)

    alpha = icon.getchannel("A")
    outline_width = max(3, round(size * 0.06))
    if outline_width % 2 == 0:
        outline_width += 1
    outline_alpha = alpha.filter(ImageFilter.MaxFilter(outline_width))

    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    outline = Image.new("RGBA", icon.size, (0, 0, 0, 0))
    outline.putalpha(outline_alpha)
    white_icon = Image.new("RGBA", icon.size, (255, 255, 255, 0))
    white_icon.putalpha(alpha)
    canvas.alpha_composite(outline, (padding, padding))
    canvas.alpha_composite(white_icon, (padding, padding))
    return canvas


def main() -> None:
    with Image.open(SOURCE) as image:
        source = image.convert("RGBA")

    frames = [outlined_icon(source, size) for size in SIZES]
    frames[-1].save(
        OUTPUT,
        format="ICO",
        append_images=frames[:-1],
        sizes=[(size, size) for size in SIZES],
    )


if __name__ == "__main__":
    main()
