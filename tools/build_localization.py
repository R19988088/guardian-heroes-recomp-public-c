#!/usr/bin/env python3
"""Build translated MRG dialogue without changing event or choice identifiers."""

import argparse
import json
from pathlib import Path
import shutil
import struct


SPEAKER = {0xE000, 0xE100, 0xE200}
LINE = 0xF700
CENTER = 0xF900


def words(data):
    return list(struct.unpack(f">{len(data) // 2}H", data))


def pack_words(values):
    return struct.pack(f">{len(values)}H", *values)


def is_command(value):
    return value in SPEAKER or 0xF000 <= value <= 0xF9FF or value == 0xFFFF


def tokens(data):
    values = words(data)
    i = 0
    while i < len(values):
        value = values[i]
        if is_command(value):
            size = 2 if value in SPEAKER else 1
            yield False, values[i:i + size]
            i += size
            continue
        text = []
        while i < len(values):
            value = values[i]
            if value == LINE:
                text.append(32)
            elif value == CENTER:
                pass
            elif is_command(value):
                break
            else:
                text.append(value)
            i += 1
        yield True, text


def events(data):
    return [tuple(value) for text, value in tokens(data)
            if not text and value[0] not in (LINE, CENTER)]


def wrapped(text, width=18):
    result = []
    line_width = 0
    closing = "，。！？；：、…）》」』】”’"
    for char in text:
        size = 1 if ord(char) < 0x100 else 2
        if char == "\n" or (line_width + size > width * 2 and char not in closing):
            result.append(LINE)
            line_width = 0
        if char != "\n":
            result.append(ord(char))
            line_width += size
    return result


def translate_message(data, translations):
    result = []
    for text, value in tokens(data):
        if not text:
            result.extend(value)
            continue
        source = pack_words(value).decode("utf-16-be").strip()
        if source:
            result.extend(wrapped(translations[source]))
        else:
            result.extend(value)
    return pack_words(result)


def translate_script(data, translations):
    count = struct.unpack_from(">I", data)[0]
    offsets = struct.unpack_from(f">{count}I", data, 4)
    output = bytearray(data[:4 + count * 4])
    for index, (start, end) in enumerate(zip(offsets, (*offsets[1:], len(data)))):
        original = data[start:end]
        translated = translate_message(original, translations)
        if events(original) != events(translated):
            raise ValueError(f"Message {index}: dialogue event sequence changed")
        struct.pack_into(">I", output, 4 + index * 4, len(output))
        output.extend(translated)
    return output


def translate_archive(data, translations):
    count = struct.unpack_from(">I", data)[0]
    header_size = (count + 1) * 32
    output = bytearray(data[:header_size])
    for index in range(count):
        offset, size = struct.unpack_from(">II", data, (index + 1) * 32)
        script = translate_script(data[offset:offset + size], translations)
        struct.pack_into(">II", output, (index + 1) * 32, len(output), len(script))
        output.extend(script)
        output.extend(b"\0" * (-len(output) % 4))
    return output


def load_translations(paths):
    translations = {}
    for path in paths:
        for row in json.loads(path.read_text()):
            if not row.get("zh", "").strip():
                raise ValueError(f"{path}: missing translation for {row['id']}")
            if any(is_command(ord(c)) for c in row["zh"]):
                raise ValueError(f"{path}: control character in translation {row['id']}")
            translations[row["en"]] = row["zh"]
    return translations


def build_menus(root):
    rows = json.loads((root / "localization/menu_zh.json").read_text())
    source_rows = json.loads((root / "localization/menu_source.json").read_text())
    patches = []
    occupied = set()
    assert len(source_rows) == len(rows)
    for original, row in zip(source_rows, rows):
        assert all(row[key] == original[key] for key in ("id", "en", "addresses"))
        assert row["zh"].strip(), row["id"]
        english = row["en"].encode("utf-16-be") + b"\0\0"
        chinese = row["zh"].encode("utf-16-be") + b"\0\0"
        assert len(chinese) <= len(english), (row["id"], row["en"], row["zh"])
        chinese = chinese.ljust(len(english), b"\0")
        for address in row["addresses"]:
            address = int(address, 16)
            span = set(range(address, address + len(english)))
            assert not span & occupied, (row["id"], hex(address))
            occupied.update(span)
            patches.append(struct.pack(">II", address, len(english)) + english + chinese)
    result = struct.pack(">II", 0x47484C31, len(patches)) + b"".join(patches)
    (root / "localization/menu_zhs.bin").write_bytes(result)
    print(f"Menus: {len(rows)} translations, {len(patches)} checked UTF-16 spans")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--install-assets", type=Path, nargs="*", default=[],
                        help="Install generated resources into these game asset roots")
    args = parser.parse_args()
    root = args.root
    translations = load_translations(sorted((root / "localization").glob("dialogue_zh_*.json")))
    result = translate_archive((root / "assets/media/script_eng.mrg").read_bytes(), translations)
    target = root / "localization/script_zhs.mrg"
    target.write_bytes(result)
    print(f"{target}: {len(translations)} translated texts, 50 scripts, {len(result)} bytes")
    build_menus(root)
    for assets in args.install_assets:
        media = assets / "media"
        if not (media / "script_eng.mrg").is_file():
            raise ValueError(f"Not a game asset directory: {assets}")
        for source, destination in (("script_zhs.mrg", "script_zhs.mrg"),
                                    ("menu_zhs.bin", "menu_zhs.bin"),
                                    ("font_zh.xprc", "font.xprc")):
            temporary = media / (destination + ".next")
            shutil.copyfile(root / "localization" / source, temporary)
            temporary.replace(media / destination)


if __name__ == "__main__":
    main()
