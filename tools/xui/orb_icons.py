#!/usr/bin/env python3
"""Pause-map orb marker scenes and images, built from scratch (no game bytes).

Each scene mirrors the shape of the native map icons (a 19.2 x 19.2
XuiScene 'map icon' inside a 640 x 480 canvas) with one XuiImage child
showing an orb drawn here. crackdown/src/hooks_orb_map.cpp loads them as
vfs://mapiconorb_agility.xur and vfs://mapiconorb_hidden.xur.
"""
import math
import struct
import zlib

if __package__:
    from . import edit, xur
else:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from tools.xui import edit, xur

# Crackdown's agility orbs glow green, hidden orbs blue.
ORBS = {
    'agility': (0x3c, 0xf0, 0x50),
    'hidden': (0x40, 0xa8, 0xff),
}
ICON = 19.2   # native map icon scene size
DOT = 12.0    # drawn orb size inside it
PIXELS = 32


def png(rgb, size=PIXELS):
    """A soft round orb: bright core, colored body, fading glow."""
    rows = []
    c = (size - 1) / 2
    for y in range(size):
        row = bytearray([0])
        for x in range(size):
            d = math.hypot(x - c, y - c) / (size / 2)
            if d <= 0.62:
                t = max(0.0, 1 - d / 0.62)
                color = [round(v + (255 - v) * 0.7 * t * t) for v in rgb]
                alpha = 255
            elif d <= 1.0:
                color = list(rgb)
                alpha = round(255 * (1 - (d - 0.62) / 0.38) ** 2)
            else:
                color, alpha = [0, 0, 0], 0
            row += bytes(color + [alpha])
        rows.append(bytes(row))

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(b''.join(rows), 9)) + chunk(b'IEND', b''))


def empty_scene():
    """Smallest valid XUIB v5 file: an anonymous XuiCanvas with no properties."""
    strings = 'XuiCanvas'.encode('utf-16-be')
    strn = struct.pack('>H', len(strings) // 2) + strings
    data = struct.pack('>HB', 1, 0)
    header = 20 + 12 * 2
    table = struct.pack('>4sII', b'STRN', header, len(strn)) + struct.pack('>4sII', b'DATA', header + len(strn), len(data))
    body = strn + data
    return struct.pack('>4sIIHIH', b'XUIB', 5, 0, 12, header + len(body), 2) + table + body


def element(editor, cls, properties):
    obj = dict(offset=None, class_index=editor.intern(cls), **{'class': cls}, flags=0, properties=[],
               property_groups=[], children=[], named_frames=[], timelines=[], id='')
    for name, value in properties:
        editor.set_property(obj, name, value)
    return obj


def icon_scene(image_name):
    editor = edit.Editor.parse(empty_scene())
    root = editor.scene.root
    editor.set_property(root, 'Width', 640.0)
    editor.set_property(root, 'Height', 480.0)
    scene = element(editor, 'XuiScene', [('Id', 'map icon'), ('Width', ICON), ('Height', ICON),
                                          ('ClassOverride', 'scene'), ('Visual', 'XuiSceneBlank')])
    offset = (ICON - DOT) / 2
    image = element(editor, 'XuiImage', [('Id', 'orb'), ('Width', DOT), ('Height', DOT),
                                          ('Position', [offset, offset, 0.0]), ('SizeMode', 1),
                                          ('ImagePath', image_name)])
    scene['children'].append(image)
    scene['flags'] |= 2
    root['children'].append(scene)
    root['flags'] |= 2
    return editor.finish()


def entries():
    """(name, bytes) pairs to add to Scene.xzp."""
    result = []
    for kind, rgb in ORBS.items():
        image = f'mapiconorb_{kind}.png'
        result += [(f'mapiconorb_{kind}.xur', icon_scene(image)), (image, png(rgb))]
    return result


if __name__ == '__main__':
    import sys
    from pathlib import Path
    out = Path(sys.argv[1] if len(sys.argv) > 1 else 'out/xui/orb-icons')
    out.mkdir(parents=True, exist_ok=True)
    for name, data in entries():
        (out / name).write_bytes(data)
        print(name, len(data))
