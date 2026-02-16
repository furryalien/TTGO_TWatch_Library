from __future__ import annotations

import argparse
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


W = 240
H = 240
CX = W // 2
CY = H // 2

DEFAULT_TRUE_UTC = True
LOCAL_TO_UTC_OFFSET_HOURS = 0
BATTERY_CAPACITY_MAH = 350.0
DEFAULT_FULL_RUNTIME_SECONDS = 24 * 60 * 60


def point_xy(angle_deg: float, radius: int) -> tuple[int, int]:
    rad = math.radians(angle_deg - 90.0)
    return int(CX + math.cos(rad) * radius), int(CY + math.sin(rad) * radius)


def pcf_weekday(day: int, month: int, year: int) -> int:
    if month < 3:
        month += 12
        year -= 1
    val = (day + (((month + 1) * 26) // 10) + year + (year // 4) + (6 * (year // 100)) + (year // 400)) % 7
    if val == 0:
        val = 7
    return (val - 1) % 7


def draw_hand(draw: ImageDraw.ImageDraw, angle_deg: float, length: int, tail: int, width: int, color: tuple[int, int, int]) -> None:
    rad = math.radians(angle_deg - 90.0)
    px = math.cos(rad + math.pi / 2.0)
    py = math.sin(rad + math.pi / 2.0)
    x1 = int(CX - math.cos(rad) * tail)
    y1 = int(CY - math.sin(rad) * tail)
    x2 = int(CX + math.cos(rad) * length)
    y2 = int(CY + math.sin(rad) * length)
    half = width // 2
    for i in range(-half, half + 1):
        ox = int(px * i)
        oy = int(py * i)
        draw.line((x1 + ox, y1 + oy, x2 + ox, y2 + oy), fill=color, width=1)


def draw_centered(draw: ImageDraw.ImageDraw, text: str, x: int, y: int, font: ImageFont.ImageFont, fill: tuple[int, int, int]) -> None:
    bbox = draw.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    draw.text((x - tw // 2, y - th // 2), text, font=font, fill=fill)


def load_font(size: int) -> ImageFont.ImageFont:
    for candidate in ["arial.ttf", "segoeui.ttf", "DejaVuSans.ttf"]:
        try:
            return ImageFont.truetype(candidate, size=size)
        except OSError:
            continue
    return ImageFont.load_default()


def estimate_battery_seconds_remaining(pct: int, charging: bool, charge_current_ma: float = 350.0, discharge_current_ma: float = 90.0) -> int:
    pct = max(0, min(100, pct))
    ratio = pct / 100.0
    seconds = ratio * DEFAULT_FULL_RUNTIME_SECONDS

    if charging:
        if charge_current_ma > 1.0:
            seconds = ((100.0 - pct) / 100.0) * BATTERY_CAPACITY_MAH / charge_current_ma * 3600.0
        else:
            seconds = (1.0 - ratio) * DEFAULT_FULL_RUNTIME_SECONDS
    else:
        if discharge_current_ma > 1.0:
            seconds = ratio * BATTERY_CAPACITY_MAH / discharge_current_ma * 3600.0

    seconds = max(0.0, min(359999.0, seconds))
    return int(seconds)


def format_hms(total_seconds: int) -> str:
    hours = total_seconds // 3600
    minutes = (total_seconds % 3600) // 60
    seconds = total_seconds % 60
    return f"{hours:02d}:{minutes:02d}:{seconds:02d}"


def draw_gmt_arrow(draw: ImageDraw.ImageDraw, angle_deg: float) -> None:
    arrow_tip_radius = 91
    arrow_base_radius = 77
    wing_half_width = 5
    arrow_color = (232, 35, 45)

    draw_hand(draw, angle_deg, arrow_base_radius, 14, 2, arrow_color)

    rad = math.radians(angle_deg - 90.0)
    perp = rad + math.pi / 2.0
    tip_x = int(CX + math.cos(rad) * arrow_tip_radius)
    tip_y = int(CY + math.sin(rad) * arrow_tip_radius)
    base_x = int(CX + math.cos(rad) * arrow_base_radius)
    base_y = int(CY + math.sin(rad) * arrow_base_radius)
    wing1_x = int(base_x + math.cos(perp) * wing_half_width)
    wing1_y = int(base_y + math.sin(perp) * wing_half_width)
    wing2_x = int(base_x - math.cos(perp) * wing_half_width)
    wing2_y = int(base_y - math.sin(perp) * wing_half_width)
    draw.polygon([(tip_x, tip_y), (wing1_x, wing1_y), (wing2_x, wing2_y)], fill=arrow_color)


def draw_face(hour: int, minute: int, second: int, day: int, month: int, year: int, true_utc: bool, gmt_offset: int, battery: int, charging: bool) -> Image.Image:
    img = Image.new("RGB", (W, H), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    c_dark = (70, 70, 70)
    c_silver = (165, 165, 165)
    c_white = (245, 245, 245)
    c_light = (190, 190, 190)

    for r, col in [(118, c_dark), (117, c_dark), (112, c_dark), (111, c_dark), (106, c_dark), (105, c_dark)]:
        draw.ellipse((CX - r, CY - r, CX + r, CY + r), outline=col)

    for h24 in range(24):
        angle = h24 * 15.0
        outer_r = 104 if h24 % 2 == 0 else 102
        inner_r = 99 if h24 % 2 == 0 else 100
        x1, y1 = point_xy(angle, inner_r)
        x2, y2 = point_xy(angle, outer_r)
        draw.line((x1, y1, x2, y2), fill=c_light, width=1)

    font_small = load_font(10)
    for h24 in range(2, 25, 2):
        angle = (h24 % 24) * 15.0
        txt = f"{h24 % 24:02d}"
        x, y = point_xy(angle, 93)
        draw_centered(draw, txt, x, y, font_small, c_light)

    for idx in range(60):
        angle = idx * 6.0
        major = idx % 5 == 0
        outer_r = 84
        inner_r = 74 if major else 79
        x1, y1 = point_xy(angle, inner_r)
        x2, y2 = point_xy(angle, outer_r)
        draw.line((x1, y1, x2, y2), fill=c_white if major else c_dark, width=1)

    for i in range(4):
        draw_hand(draw, i * 90.0, 83, -74, 3, c_white)

    font_big = load_font(15)
    draw_centered(draw, "12", CX, CY - 58, font_big, c_white)
    draw_centered(draw, "9", CX - 66, CY - 9, font_big, c_white)
    draw_centered(draw, "6", CX, CY + 58, font_big, c_white)
    draw_centered(draw, "3", CX + 66, CY - 9, font_big, c_white)

    win_cx, win_cy = 169, 161
    draw_centered(draw, f"{day:02d}", win_cx, win_cy, load_font(14), (255, 255, 255))

    days2 = ["SU", "MO", "TU", "WE", "TH", "FR", "SA"]
    weekday = pcf_weekday(day, month, year)
    wx, wy = point_xy(225.0, 62)
    draw_centered(draw, days2[weekday], wx, wy, load_font(14), (255, 255, 255))

    # Upper-right power indicator
    bx, by, bw, bh = 205, 10, 14, 6
    battery = max(0, min(100, battery))
    draw.rectangle((bx, by, bx + bw, by + bh), outline=(255, 255, 255), width=1)
    draw.rectangle((bx + bw, by + 2, bx + bw + 2, by + 5), fill=(255, 255, 255))
    fill_w = int((bw - 4) * battery / 100.0)
    fill_col = (255, 255, 255) if battery > 20 else (220, 40, 40)
    if fill_w > 0:
        draw.rectangle((bx + 2, by + 2, bx + 2 + fill_w, by + bh - 2), fill=fill_col)
    if charging:
        bolt = [(bx + 7, by + 1), (bx + 5, by + 3), (bx + 8, by + 3), (bx + 6, by + 5)]
        draw.line(bolt, fill=(240, 220, 40), width=1)
    remain_text = format_hms(estimate_battery_seconds_remaining(battery, charging))
    draw.text((W - 2 - (len(remain_text) * 6), by + bh + 4), remain_text, font=load_font(9), fill=(255, 255, 255))

    second_angle = second * 6.0
    minute_angle = (minute + second / 60.0) * 6.0
    hour_angle = ((hour % 12) + minute / 60.0 + second / 3600.0) * 30.0

    applied_offset = LOCAL_TO_UTC_OFFSET_HOURS if true_utc else gmt_offset
    gmt_hour = (hour + applied_offset) % 24
    gmt_angle = (gmt_hour + minute / 60.0 + second / 3600.0) * 15.0

    draw_gmt_arrow(draw, gmt_angle)
    draw_hand(draw, hour_angle, 51, 11, 4, c_white)
    draw_hand(draw, minute_angle, 73, 13, 2, c_white)
    draw_hand(draw, second_angle, 87, 16, 1, c_light)

    draw.ellipse((CX - 5, CY - 5, CX + 5, CY + 5), fill=c_white)
    draw.ellipse((CX - 3, CY - 3, CX + 3, CY + 3), fill=(0, 0, 0))
    draw.ellipse((CX - 6, CY - 6, CX + 6, CY + 6), outline=(232, 35, 45))

    return img


def main() -> None:
    parser = argparse.ArgumentParser(description="Render GMT watch face preview image")
    parser.add_argument("--hour", type=int, default=10)
    parser.add_argument("--minute", type=int, default=10)
    parser.add_argument("--second", type=int, default=28)
    parser.add_argument("--day", type=int, default=28)
    parser.add_argument("--month", type=int, default=2)
    parser.add_argument("--year", type=int, default=2026)
    parser.add_argument("--true-utc", action="store_true", default=DEFAULT_TRUE_UTC)
    parser.add_argument("--gmt-offset", type=int, default=0)
    parser.add_argument("--battery", type=int, default=78)
    parser.add_argument("--charging", action="store_true")
    parser.add_argument("--out", type=Path, default=Path("images/GMTWatchFace_preview.png"))
    args = parser.parse_args()

    img = draw_face(args.hour, args.minute, args.second, args.day, args.month, args.year, args.true_utc, args.gmt_offset, args.battery, args.charging)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    img.save(args.out)
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
