from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


WIDTH = 2400
HEIGHT = 1800

RED = "#c7372f"
BLACK = "#222222"
BLUE = "#1c64a0"
GREEN = "#2f7d57"
INK = "#172026"
MUTED = "#4d5a61"
BG = "#f7f8f4"


SERVOS_LEFT = [
    ("Left Drive MG996", "continuous rotation", "GPIO18", 100, 800),
    ("Right Drive MG996", "continuous rotation", "GPIO19", 100, 1000),
    ("Arm Base MG996", "arm servo", "GPIO21", 100, 1200),
]

SERVOS_RIGHT = [
    ("Arm Shoulder SG90", "arm servo", "GPIO22", 1830, 720),
    ("Arm Elbow SG90", "arm servo", "GPIO23", 1830, 920),
    ("Arm Wrist SG90", "arm servo", "GPIO25", 1830, 1120),
    ("Arm Claw SG90", "arm servo", "GPIO26", 1830, 1320),
]


def font(size, bold=False):
    candidates = [
        "C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf",
    ]
    for candidate in candidates:
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            pass
    return ImageFont.load_default()


def esc(text):
    return (
        str(text)
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


class Svg:
    def __init__(self):
        self.parts = [
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}">',
            f'<rect x="0" y="0" width="{WIDTH}" height="{HEIGHT}" fill="{BG}"/>',
        ]

    def rect(self, x, y, w, h, fill="#ffffff", stroke=INK, width=4, radius=16):
        self.parts.append(
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>'
        )

    def text(self, x, y, value, size=30, color=INK, weight="400", anchor="start"):
        self.parts.append(
            f'<text x="{x}" y="{y}" font-family="Arial, Helvetica, sans-serif" font-size="{size}" '
            f'font-weight="{weight}" fill="{color}" text-anchor="{anchor}">{esc(value)}</text>'
        )

    def line(self, points, color=BLACK, width=5, dash=None):
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
        if len(points) == 2:
            (x1, y1), (x2, y2) = points
            self.parts.append(
                f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" stroke-width="{width}" '
                f'stroke-linecap="round"{dash_attr}/>'
            )
        else:
            path = " ".join(f"{x},{y}" for x, y in points)
            self.parts.append(
                f'<polyline points="{path}" fill="none" stroke="{color}" stroke-width="{width}" '
                f'stroke-linecap="round" stroke-linejoin="round"{dash_attr}/>'
            )

    def dot(self, x, y, color):
        self.parts.append(f'<circle cx="{x}" cy="{y}" r="8" fill="{color}"/>')

    def finish(self):
        self.parts.append("</svg>")
        return "\n".join(self.parts) + "\n"


def png_text(draw, x, y, value, size=30, fill=INK, bold=False, anchor=None):
    draw.text((x, y), value, font=font(size, bold), fill=fill, anchor=anchor)


def png_rect(draw, x, y, w, h, fill="#ffffff", outline=INK, width=4, radius=16):
    draw.rounded_rectangle((x, y, x + w, y + h), radius=radius, fill=fill, outline=outline, width=width)


def draw_component_svg(svg, x, y, w, h, title, lines, fill="#ffffff", stroke=INK):
    svg.rect(x, y, w, h, fill, stroke)
    svg.text(x + w / 2, y + 50, title, 34, INK, "700", "middle")
    for index, line in enumerate(lines):
        svg.text(x + 34, y + 95 + index * 36, line, 26, MUTED)


def draw_component_png(draw, x, y, w, h, title, lines, fill="#ffffff", outline=INK):
    png_rect(draw, x, y, w, h, fill, outline)
    png_text(draw, x + w // 2, y + 28, title, 34, bold=True, anchor="ma")
    for index, line in enumerate(lines):
        png_text(draw, x + 34, y + 82 + index * 36, line, 26, fill=MUTED)


def draw_servo_svg(svg, name, kind, gpio, x, y):
    draw_component_svg(
        svg,
        x,
        y,
        470,
        160,
        name,
        [f"Signal: {gpio}", "Power: +6V and GND"],
        "#ffffff",
        "#5d686e",
    )


def draw_servo_png(draw, name, kind, gpio, x, y):
    draw_component_png(
        draw,
        x,
        y,
        470,
        160,
        name,
        [f"Signal: {gpio}", "Power: +6V and GND"],
        "#ffffff",
        "#5d686e",
    )


def draw_schematic_svg(path):
    svg = Svg()

    svg.text(WIDTH / 2, 70, "ESP32 Robot Arm Rover Wiring Schematic", 48, INK, "700", "middle")
    svg.text(
        WIDTH / 2,
        112,
        "3.7 V Li-ion battery -> DC-DC boost converter -> 6 V rail for servos and ESP32 VIN/5V",
        27,
        MUTED,
        "400",
        "middle",
    )

    draw_component_svg(svg, 100, 170, 350, 190, "3.7 V Li-ion", ["Battery +", "Battery -"], "#fff8e8", "#8a6d23")
    draw_component_svg(svg, 580, 145, 440, 240, "DC-DC Converter", ["IN+ and IN-", "OUT+ and OUT-", "Adjust output to 6 V"], "#eef6ff", "#245c9c")
    draw_component_svg(
        svg,
        1230,
        130,
        540,
        310,
        "ESP32 DevKit",
        ["VIN/5V: connect to +6V rail*", "GND: connect to common ground", "GPIO pins: servo PWM signals", "Wi-Fi AP hosts control page"],
        "#edf7ef",
        GREEN,
    )

    svg.line([(450, 225), (580, 225)], RED, 8)
    svg.line([(450, 305), (580, 305)], BLACK, 8)
    svg.text(480, 208, "Battery + to IN+", 25, RED, "700")
    svg.text(480, 338, "Battery - to IN-", 25, BLACK, "700")

    svg.line([(1020, 220), (1110, 220), (1110, 560)], RED, 8)
    svg.line([(1020, 305), (1160, 305), (1160, 635)], BLACK, 8)
    svg.text(1040, 200, "OUT+ 6 V", 25, RED, "700")
    svg.text(1040, 340, "OUT- GND", 25, BLACK, "700")

    svg.text(120, 535, "Power rails", 34, INK, "700")
    svg.line([(120, 560), (2280, 560)], RED, 10)
    svg.line([(120, 635), (2280, 635)], BLACK, 10)
    svg.text(140, 548, "+6 V servo rail", 27, RED, "700")
    svg.text(140, 680, "Common GND rail", 27, BLACK, "700")

    svg.line([(1460, 560), (1460, 440)], RED, 7)
    svg.line([(1530, 635), (1530, 440)], BLACK, 7)
    svg.text(1478, 500, "VIN/5V*", 25, RED, "700")
    svg.text(1548, 500, "GND", 25, BLACK, "700")

    for name, kind, gpio, x, y in SERVOS_LEFT + SERVOS_RIGHT:
        draw_servo_svg(svg, name, kind, gpio, x, y)

    for _name, _kind, _gpio, x, y in SERVOS_LEFT:
        svg.line([(70, 560), (70, y + 52), (x, y + 52)], RED, 5)
        svg.line([(90, 635), (90, y + 98), (x, y + 98)], BLACK, 5)
        svg.dot(x, y + 52, RED)
        svg.dot(x, y + 98, BLACK)

    for _name, _kind, _gpio, x, y in SERVOS_RIGHT:
        right = x + 470
        svg.line([(2330, 560), (2330, y + 52), (right, y + 52)], RED, 5)
        svg.line([(2310, 635), (2310, y + 98), (right, y + 98)], BLACK, 5)
        svg.dot(right, y + 52, RED)
        svg.dot(right, y + 98, BLACK)

    left_signal_y = [220, 265, 310]
    for index, (_name, _kind, _gpio, x, y) in enumerate(SERVOS_LEFT):
        signal_y = y + 130
        svg.line([(1230, left_signal_y[index]), (1140, left_signal_y[index]), (1140, signal_y), (x + 470, signal_y)], BLUE, 5, "14 12")
        svg.dot(x + 470, signal_y, BLUE)

    right_signal_y = [220, 260, 300, 340]
    for index, (_name, _kind, _gpio, x, y) in enumerate(SERVOS_RIGHT):
        signal_y = y + 130
        svg.line([(1770, right_signal_y[index]), (1800, right_signal_y[index]), (1800, signal_y), (x, signal_y)], BLUE, 5, "14 12")
        svg.dot(x, signal_y, BLUE)

    svg.text(120, 1640, "Wire colors in this diagram: red = +6 V, black = ground, blue dashed = ESP32 servo signal.", 28, MUTED)
    svg.text(120, 1690, "*Check your ESP32 DevKit regulator before feeding 6 V into VIN/5V. Use a separate 5 V regulator if your board requires it.", 26, "#7a2f2f")
    svg.text(120, 1730, "Do not power the servos from the ESP32 3.3 V pin. All servo grounds and ESP32 ground must be connected together.", 26, "#7a2f2f")

    path.write_text(svg.finish(), encoding="utf-8")
    print(f"Wrote {path}")


def draw_schematic_png(path):
    img = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(img)

    png_text(draw, WIDTH // 2, 32, "ESP32 Robot Arm Rover Wiring Schematic", 48, bold=True, anchor="ma")
    png_text(
        draw,
        WIDTH // 2,
        86,
        "3.7 V Li-ion battery -> DC-DC boost converter -> 6 V rail for servos and ESP32 VIN/5V",
        27,
        fill=MUTED,
        anchor="ma",
    )

    draw_component_png(draw, 100, 170, 350, 190, "3.7 V Li-ion", ["Battery +", "Battery -"], "#fff8e8", "#8a6d23")
    draw_component_png(draw, 580, 145, 440, 240, "DC-DC Converter", ["IN+ and IN-", "OUT+ and OUT-", "Adjust output to 6 V"], "#eef6ff", "#245c9c")
    draw_component_png(
        draw,
        1230,
        130,
        540,
        310,
        "ESP32 DevKit",
        ["VIN/5V: connect to +6V rail*", "GND: connect to common ground", "GPIO pins: servo PWM signals", "Wi-Fi AP hosts control page"],
        "#edf7ef",
        GREEN,
    )

    draw.line([(450, 225), (580, 225)], fill=RED, width=8)
    draw.line([(450, 305), (580, 305)], fill=BLACK, width=8)
    png_text(draw, 480, 194, "Battery + to IN+", 25, fill=RED, bold=True)
    png_text(draw, 480, 318, "Battery - to IN-", 25, fill=BLACK, bold=True)

    draw.line([(1020, 220), (1110, 220), (1110, 560)], fill=RED, width=8)
    draw.line([(1020, 305), (1160, 305), (1160, 635)], fill=BLACK, width=8)
    png_text(draw, 1040, 188, "OUT+ 6 V", 25, fill=RED, bold=True)
    png_text(draw, 1040, 318, "OUT- GND", 25, fill=BLACK, bold=True)

    png_text(draw, 120, 500, "Power rails", 34, bold=True)
    draw.line([(120, 560), (2280, 560)], fill=RED, width=10)
    draw.line([(120, 635), (2280, 635)], fill=BLACK, width=10)
    png_text(draw, 140, 522, "+6 V servo rail", 27, fill=RED, bold=True)
    png_text(draw, 140, 655, "Common GND rail", 27, fill=BLACK, bold=True)

    draw.line([(1460, 560), (1460, 440)], fill=RED, width=7)
    draw.line([(1530, 635), (1530, 440)], fill=BLACK, width=7)
    png_text(draw, 1478, 476, "VIN/5V*", 25, fill=RED, bold=True)
    png_text(draw, 1548, 476, "GND", 25, fill=BLACK, bold=True)

    for name, kind, gpio, x, y in SERVOS_LEFT + SERVOS_RIGHT:
        draw_servo_png(draw, name, kind, gpio, x, y)

    for _name, _kind, _gpio, x, y in SERVOS_LEFT:
        draw.line([(70, 560), (70, y + 52), (x, y + 52)], fill=RED, width=5)
        draw.line([(90, 635), (90, y + 98), (x, y + 98)], fill=BLACK, width=5)
        draw.ellipse((x - 8, y + 44, x + 8, y + 60), fill=RED)
        draw.ellipse((x - 8, y + 90, x + 8, y + 106), fill=BLACK)

    for _name, _kind, _gpio, x, y in SERVOS_RIGHT:
        right = x + 470
        draw.line([(2330, 560), (2330, y + 52), (right, y + 52)], fill=RED, width=5)
        draw.line([(2310, 635), (2310, y + 98), (right, y + 98)], fill=BLACK, width=5)
        draw.ellipse((right - 8, y + 44, right + 8, y + 60), fill=RED)
        draw.ellipse((right - 8, y + 90, right + 8, y + 106), fill=BLACK)

    left_signal_y = [220, 265, 310]
    for index, (_name, _kind, _gpio, x, y) in enumerate(SERVOS_LEFT):
        signal_y = y + 130
        draw.line([(1230, left_signal_y[index]), (1140, left_signal_y[index]), (1140, signal_y), (x + 470, signal_y)], fill=BLUE, width=5)
        draw.ellipse((x + 462, signal_y - 8, x + 478, signal_y + 8), fill=BLUE)

    right_signal_y = [220, 260, 300, 340]
    for index, (_name, _kind, _gpio, x, y) in enumerate(SERVOS_RIGHT):
        signal_y = y + 130
        draw.line([(1770, right_signal_y[index]), (1800, right_signal_y[index]), (1800, signal_y), (x, signal_y)], fill=BLUE, width=5)
        draw.ellipse((x - 8, signal_y - 8, x + 8, signal_y + 8), fill=BLUE)

    png_text(draw, 120, 1610, "Wire colors in this diagram: red = +6 V, black = ground, blue = ESP32 servo signal.", 28, fill=MUTED)
    png_text(draw, 120, 1660, "*Check your ESP32 DevKit regulator before feeding 6 V into VIN/5V. Use a separate 5 V regulator if your board requires it.", 26, fill="#7a2f2f")
    png_text(draw, 120, 1700, "Do not power the servos from the ESP32 3.3 V pin. All servo grounds and ESP32 ground must be connected together.", 26, fill="#7a2f2f")

    img.save(path)
    print(f"Wrote {path}")


def main():
    out_dir = Path(__file__).resolve().parent
    draw_schematic_svg(out_dir / "robot_arm_rover_schematic.svg")
    draw_schematic_png(out_dir / "robot_arm_rover_schematic.png")


if __name__ == "__main__":
    main()
