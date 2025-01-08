import argparse
import json
import subprocess
import sys
from typing import List, Optional


def call_process_get_output(cli: List[str]) -> str:
    process = subprocess.Popen(cli, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    process.wait()
    output, _errors = process.communicate()
    return output.decode()


class I3Module:
    def __init__(
        self,
        full_text: str,
        name: str,
        color: Optional[str] = None,
        markup: Optional[str] = None,
    ):
        self.full_text = full_text
        self.name = name
        if color:
            self.color = color
        if markup:
            self.markup = markup


def get_bluetooth_battery(
    is_debug: bool,
    battery_red: int,
    battery_yellow: int,
) -> List[I3Module]:
    devices_output = call_process_get_output(["bluetoothctl", "devices"]).splitlines()
    device_name_id_map = {
        x[1].strip(): x[0].strip()
        for x in map(
            lambda x: [x[: x.find(" ")], x[x.find(" ") + 1 :]],
            map(lambda x: x.lstrip("Device").strip(), devices_output),
        )
    }

    resp = []
    for device_name, device_id in device_name_id_map.items():
        device_info = call_process_get_output(
            ["bluetoothctl", "info", device_id]
        ).splitlines()

        def find_and_clean_up(info, to_find):
            return map(
                lambda x: x[x.find(": ") + 1 :].strip(),
                filter(lambda x: x.find(to_find) != -1, info),
            )

        icon = next(find_and_clean_up(device_info, "Icon"), None)
        is_connected = next(find_and_clean_up(device_info, "Connected"), "no")
        is_connected = is_connected == "yes"
        battery = next(
            map(
                lambda x: float(x[x.find("(") + 1 : x.find(")")]),
                find_and_clean_up(device_info, "Battery Percentage"),
            ),
            None,
        )

        if is_debug:
            print(
                f"name: {device_name}, icon: {icon}, connected: {is_connected}, battery: {battery}"
            )

        icon_name_symbol = {
            "input-mouse": "󰍽",
            "input-keyboard": "",
            "audio-headset": "",
            "audio-headphones": "",
        }

        if icon:
            icon = icon_name_symbol.get(icon)

        color = None
        if is_connected and battery is not None:
            battery_text = f"{battery:.0f}%"
            if icon:
                battery_text = f"{icon} {battery_text}"
            else:
                battery_text = f"{device_name} {battery_text}"

            if battery_red <= battery <= battery_yellow:
                color = "#FFFF00"
                battery_text = f"{battery_text} 󰥄"
            elif battery < battery_red:
                color = "#FF0000"
                battery_text = f"{battery_text} 󰤾"

            resp.append(
                I3Module(
                    full_text=battery_text,
                    name=f"bluetooth_battery_{device_name}",
                    color=color,
                )
            )

    return resp


def print_line(message):
    """Non-buffered printing to stdout."""
    sys.stdout.write(message + "\n")
    sys.stdout.flush()


def read_line():
    """Interrupted respecting reader for stdin."""
    # try reading a line, removing any extra whitespace
    try:
        line = sys.stdin.readline().strip()
        # i3status sends EOF, or an empty line
        if not line:
            sys.exit(3)
        return line
    # exit on ctrl-c
    except KeyboardInterrupt:
        sys.exit()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--debug", "-d", help="Run in debug mode", action="store_true")
    parser.add_argument(
        "--battery-red", help="Battery level to print red", type=int, default=20
    )
    parser.add_argument(
        "--battery-yellow", help="Battery level to print red", type=int, default=50
    )
    args = parser.parse_args()

    if args.debug:
        i3_modules = get_bluetooth_battery(
            is_debug=args.debug,
            battery_yellow=args.battery_yellow,
            battery_red=args.battery_red,
        )
        print(f"args: {args}")
        print(i3_modules)
    else:
        # Skip the first line which contains the version header.
        print_line(read_line())

        # The second line contains the start of the infinite array.
        print_line(read_line())

        while True:
            line, prefix = read_line(), ""
            # ignore comma at start of lines
            if line.startswith(","):
                line, prefix = line[1:], ","

            i3_modules = get_bluetooth_battery(
                is_debug=args.debug,
                battery_yellow=args.battery_yellow,
                battery_red=args.battery_red,
            )

            j = json.loads(line)
            # insert information into the start of the json, but could be anywhere
            # CHANGE THIS LINE TO INSERT SOMETHING ELSE

            for i3_module in i3_modules:
                j.insert(0, i3_module.__dict__)
            # and echo back new encoded json
            print_line(prefix + json.dumps(j))
