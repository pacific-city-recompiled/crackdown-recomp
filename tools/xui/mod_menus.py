#!/usr/bin/env python3
"""Build original-verified PC menu packages and audited patches under out/xui.

Run as: python3 -m tools.xui.mod_menus ~/DecompProject/game
No input is changed. New labels are English in every supplied locale.
"""
import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import subprocess
import sys

if __package__:
    from . import edit, xpatch, xur, xus, xzp
else:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from tools.xui import edit, xpatch, xur, xus, xzp

REPO = Path(__file__).resolve().parents[2]
MANIFEST_SOURCE = 'HEAD:tools/game-manifest.txt'


def manifest_records(text):
    records = {}
    for line in text.splitlines():
        if not line or line.startswith('#'):
            continue
        parts = line.split('\t')
        if len(parts) != 3 or parts[0] in records:
            raise ValueError('invalid or duplicate manifest row')
        path, size, sha = parts
        if path.startswith('/') or '\\' in path or any(p in ('', '.', '..') for p in path.split('/')):
            raise ValueError('unsafe manifest path')
        if sha != '-' and (len(sha) != 64 or any(c not in '0123456789abcdef' for c in sha)):
            raise ValueError('invalid manifest SHA256')
        records[path] = (int(size), sha)
    return records


def verify_inputs(game, records):
    """Verify all recipe inputs before parsing, modifying or writing any output."""
    game = game.resolve()
    paths = [game/'XUIFrontEnd/menuHUD_3.xzp', game/'XUIFrontEnd/Scene.xzp',
             game/'Local/en-US/XUIFrontEnd/Scene.xzp']
    paths += sorted((game/'Local').glob('*/XUIFrontEnd/Scene.xzp'))
    inputs = {}
    for path in dict.fromkeys(paths):
        if not path.resolve().is_relative_to(game):
            raise ValueError('game input symlink escapes game root')
        rel = path.relative_to(game).as_posix()
        expected = records.get(rel)
        if expected is None or expected[1] == '-':
            raise ValueError(f'no trusted original SHA256 for {rel} in {MANIFEST_SOURCE}')
        original = path.read_bytes()
        if len(original) != expected[0] or hashlib.sha256(original).hexdigest() != expected[1]:
            raise ValueError(f'original size/SHA256 mismatch: {rel}')
        inputs[rel] = original
    return inputs


def nav_cycle(editor, controls):
    for i, obj in enumerate(controls):
        editor.set_property(obj, 'NavUp', controls[i-1]['id'])
        editor.set_property(obj, 'NavDown', controls[(i+1) % len(controls)]['id'])


def add_pc_settings(editor):
    tab = editor.find('Menu/Menu/Tab1')
    source = editor.find('Menu/Menu/Tab1/displaySetup')
    clone = editor.clone(source, tab, 'pcSettings', tab['children'].index(source)+1)
    editor.set_property(clone, 'PressPath', '')
    editor.set_property(clone, 'Text', 'PC Settings')
    editor.clone_timelines(tab, source, clone)
    ids = ['controllerSettings', 'liveSetup', 'audioSetup', 'displaySetup', 'pcSettings', 'gameCredits']
    controls = [editor.find('Menu/Menu/Tab1/'+name) for name in ids]
    nav_cycle(editor, controls)
    # Five original 37px rows at 38px pitch occupy 189px. Six rows at 31.5px
    # height/pitch occupy the same 189px, leaving the header/footer untouched.
    for i, obj in enumerate(controls):
        editor.set_property(obj, 'Position', [259.0, 372.6 + 31.5*i, 0.0])
        editor.set_property(obj, 'Height', 31.5)


def add_quit_game(editor, order, top, pitch):
    """Clone Tab2/quit as quitGame below it; order lists Tab2's rows top to bottom."""
    tab2 = editor.find('Menu/Menu/Tab2')
    quit = editor.find('Menu/Menu/Tab2/quit')
    desktop = editor.clone(quit, tab2, 'quitGame', tab2['children'].index(quit)+1)
    for name, value in [('PressPath', ''), ('Text', 'Quit to Desktop')]:
        editor.set_property(desktop, name, value)
    editor.clone_timelines(tab2, quit, desktop)
    controls2 = [editor.find('Menu/Menu/Tab2/'+name) for name in order + ['quitGame']]
    nav_cycle(editor, controls2)
    for i, obj in enumerate(controls2):
        editor.set_property(obj, 'Position', [515.0, top + pitch*i, 0.0])
        editor.set_property(obj, 'Height', pitch)
    editor.set_property(editor.find('Menu/Menu/Tab2/mainMenuButton'), 'NavUp', 'quitGame')


def modify_menu(data):
    editor = edit.Editor.parse(data)
    add_pc_settings(editor)
    add_quit_game(editor, ['campaign', 'timeTrials', 'DLC', 'extract', 'quit'], 372.6, 31.5)
    return editor.finish()


# Free-For-All Pack (Marketplace DLC) XUIFrontEnd/Scene_CommonDLC.xzp. With the
# title update the game loads the pause menu from this package's menuHud_3.xur
# instead of the disc's menuHUD_3.xzp. Its Tab2 has seven 37px rows at 38px pitch
# (296.6 to 561.6 px, Street Racing and Keys to the City added); Tab1 matches the disc.
DLC_MENU_SHA256 = '66f67cd7b0803d67dd05d578babe0abbe143e1c4aab7355ed381980d7aa5d9bf'
DLC_TAB2 = ['campaign', 'timeTrials', 'StreetRacing', 'CheatMode', 'DLC', 'extract', 'quit']


def modify_dlc_menu(data):
    editor = edit.Editor.parse(data)
    add_pc_settings(editor)
    # Eight rows in the same 265px the seven original rows occupy.
    add_quit_game(editor, DLC_TAB2, 296.6, 265.0 / 8)
    return editor.finish()


# PC Settings rows, in page order. Ids and button order must match
# crackdown/src/pc_settings.cpp (Rows()).
ROWS = [
    ('framerate', 'Frame rate', ['60 fps', '30 fps', '120 fps', '144 fps']),
    ('resolution', 'Internal resolution', ['720p', '1440p', '4K']),
    ('aspect', 'Aspect ratio', ['16:9', '16:10', '21:9', '32:9', '4:3']),
    ('antialiasing', 'Anti-aliasing', ['Off', 'FXAA']),
    ('filtering', 'Texture filtering', ['Game', '2x', '4x', '8x', '16x']),
    ('display', 'Display', ['Fullscreen', 'Windowed']),
]
# Row and choice names follow the launcher's (crackdown/launcher/main.cpp) without its
# "(default)" / "(experimental)" notes, "Anisotropic" and "'s own", which do not fit a 5-button row.
ROW_TOP = 219.0    # first radio group (351 with three rows, 287 with five)
NARROW_GAP = 6.0  # between radio buttons in rows of four or more
ROW_PITCH = 68.0   # was 60; 36 (row) + LABEL_ABOVE + 2 px gap
# Caption text is drawn from the top of its box (PC checks 4-4c): 26/28-high boxes cut
# descenders, and a label 36 above its row overlapped the row above at 64 pitch.
LABEL_ABOVE = 30.0


def add_radio_buttons(editor, group, count):
    """Grow a cloned preset group (XuiRadioButton1/2) to count buttons.

    The preset's two buttons fill the panel width, so three or more buttons are
    narrowed to share that same span with the original gap between them
    (PC check 4, 2026-10-18: a third button at the old step sat outside the
    panel). Rows of four or more close the gap to NARROW_GAP so their labels fit
    (PC check 2026-10-30: "120 fps" ran past its button at the 20 gap).
    NavLeft/NavRight wrap around the row.
    """
    buttons = group['children']
    if count < len(buttons):
        raise ValueError('radio rows need at least the two preset buttons')
    value = lambda b, name: next((p['value'] for p in b['properties'] if p['name'] == name), None)
    if count > len(buttons):
        first, second = value(buttons[0], 'Position'), value(buttons[1], 'Position')
        width = value(buttons[0], 'Width')
        if first is None or second is None or width is None:
            raise ValueError('preset radio buttons have no Position/Width to lay out')
        gap = (second[0] - first[0]) - width
        if count >= 4:
            gap = min(gap, NARROW_GAP)
        new_width = (second[0] - first[0] + width - (count - 1) * gap) / count
        while len(group['children']) < count:
            previous = group['children'][-1]
            button = editor.clone(previous, group, f'XuiRadioButton{len(group["children"]) + 1}')
            try:
                editor.clone_timelines(group, previous, button)
            except ValueError:
                pass  # no group-owned animations for the buttons
        for i, button in enumerate(group['children']):
            editor.set_property(button, 'Width', new_width)
            editor.set_property(button, 'Position', [first[0] + i * (new_width + gap)] + list(first[1:]))
    buttons = group['children']
    for i, button in enumerate(buttons):  # wraps, like the original pair
        editor.set_property(button, 'NavRight', buttons[(i + 1) % len(buttons)]['id'])
        editor.set_property(button, 'NavLeft', buttons[i - 1]['id'])


def pc_scene(data):
    """Screen 26: native DisplayOptions chrome, five radio rows, safe ctor dummies.

    Static evidence: claude_screen.md (2026-10-12), ctor 0x8266B928.
    Native input/visual behavior is unverified at runtime until the PC receipt.
    """
    editor = edit.Editor.parse(data)
    page = editor.find('DisplayOptions')
    editor.set_property(page, 'Id', 'options')
    editor.set_property(page, 'ClassOverride', 'scene')
    editor.set_property(page, 'DefaultFocus', 'framerate')
    for name in ('NavLeft', 'NavRight', 'NavUp', 'NavDown'):
        editor.set_property(page, name, 'options')
    title = editor.find('options/pageTitle')
    for name, value in [('Text', 'PC Settings'), ('Width', 590.0),
                        ('Height', 40.0), ('Visual', 'XuiLabel'), ('PointSize', 24.0)]:
        editor.set_property(title, name, value)
    # Make room above the original controls; keep the native collapse animation.
    # Six rows instead of three (ROW_PITCH apart), so the title and background
    # start EXTRA higher and the background is taller by the added rows.
    extra = 146.0
    editor.set_property(title, 'Position', [346.0, 289.0 - extra, 0.0])
    background = editor.find('options/background2')
    last = ROW_TOP + (len(ROWS) - 1) * ROW_PITCH
    # Grow by how far the Back button moved down (was at 575) plus the raised top.
    editor.set_property(background, 'Height', 282.0 + (last + 104.0 - 575.0) + extra)
    editor.set_property(background, 'Position', [336.0, 422.5 - extra, 0.0])
    for timeline in page['timelines']:
        if timeline['target'] in ('pageTitle', 'background2'):
            for key in timeline['keyframes']:
                for i, prop in enumerate(key['properties']):
                    if prop['name'] == 'Position':
                        position = list(prop['value'])
                        position[1] -= 44.0 + extra
                        key['properties'][i] = editor.make_property('Position', 'v', position)

    def caption(ident, text, y, size=20.0):
        label = editor.clone(title, page, ident)
        # This game's Element class predates Hittable/DisableFocusRecursion.
        # Use verified Control fields; unsupported bits desynchronize XUR load.
        for name, value in [('Text', text), ('Width', 590.0), ('Visual', 'XuiLabel'),
                            ('Height', float(round(size * 1.7))), ('PointSize', size), ('Position', [354.0, y, 0.0]),
                            ('Enabled', False), ('UnfocusedInput', False)]:
            editor.set_property(label, name, value)
        # Position and any native focus/nav keys must not restore old rows.
        for anim in editor.clone_timelines(page, title, label):
            keep = [i for i, d in enumerate(anim['definitions']) if d['name'] == 'Opacity']
            anim['definitions'] = [anim['definitions'][i] for i in keep]
            for key in anim['keyframes']:
                key['properties'] = [key['properties'][i] for i in keep]
        return label

    preset = editor.find('options/preset')
    controls = []
    for row, (ident, label, texts) in enumerate(ROWS):
        y = ROW_TOP + row * ROW_PITCH
        group = editor.clone(preset, page, ident)
        editor.set_property(group, 'Position', [345.0, y, 0.0])
        editor.set_property(group, 'Height', 36.0)
        for anim in editor.clone_timelines(page, preset, group):
            keep = [i for i, d in enumerate(anim['definitions']) if d['name'] == 'Opacity']
            anim['definitions'] = [anim['definitions'][i] for i in keep]
            for key in anim['keyframes']:
                key['properties'] = [key['properties'][i] for i in keep]
        add_radio_buttons(editor, group, len(texts))
        for button, text in zip(group['children'], texts):
            editor.set_property(button, 'Text', text)
        controls.append(group)
        caption(ident+'Label', label, y-LABEL_ABOVE)
    nav_cycle(editor, controls)
    for group in controls:
        for button in group['children']:
            editor.set_property(button, 'NavUp', edit.property_value(group, 'NavUp'))
            editor.set_property(button, 'NavDown', edit.property_value(group, 'NavDown'))
    caption('restartNotice', 'Changes apply the next time the game starts.', last + 54.0, 18.0)

    # The options ctor ALWAYS subscribes AudioSetup+52 and conditionally hides
    # AgentProfiles. Missing wrappers are dereferenced; disabled real wrappers
    # are required even though neither dummy is ever navigated to.
    agent = editor.clone(title, page, 'AgentProfiles')
    audio = editor.clone(editor.find('options/reset_button'), page, 'AudioSetup')
    for dummy in (agent, audio):
        for name, value in [('Text', ''), ('Show', False),
                            ('Opacity', 0.0), ('Enabled', False),
                            ('UnfocusedInput', False)]:
            editor.set_property(dummy, name, value)
        for name in ('NavLeft', 'NavRight', 'NavUp', 'NavDown'):
            editor.set_property(dummy, name, '')
    editor.set_property(audio, 'PressPath', '')
    # No footer helper exists on screen 26: hide unset DisplayOptions hints.
    editor.set_property(editor.find('options/footer'), 'Show', False)
    # Retain the native XuiBackButton/B key and give mouse users a visible Back.
    back = editor.find('options/XuiBackButton1')
    for name, value in [('Text', 'Back'), ('Opacity', 1.0),
                        ('Visual', 'new-subMenuButton2'), ('Width', 160.0), ('Height', 36.0),
                        ('Position', [354.0, last + 104.0, 0.0]), ('NavUp', ROWS[-1][0]),
                        ('NavDown', ROWS[0][0])]:
        editor.set_property(back, name, value)
    retained = {'XuiBackButton1', 'footer', 'background2', 'pageTitle', 'AgentProfiles',
                'AudioSetup', 'restartNotice'}
    retained |= {ident for ident, _, _ in ROWS} | {ident + 'Label' for ident, _, _ in ROWS}
    page['children'] = [c for c in page['children'] if c['id'] in retained]
    page['timelines'] = [t for t in page['timelines'] if t['target'] in retained]
    return editor.finish()


def pc_localization(data):
    """Future options.xus: preserve chrome metrics, supply English new paths.

    These localization patches are audited recipe outputs, not shipped yet.
    """
    records = []
    chrome = {'pageTitle', 'background2', 'XuiBackButton1'}
    for record in xus.parse(data):
        parts = record['path'].split('.')
        if parts[0] != 'DisplayOptions':
            raise ValueError('unexpected DisplayOptions localization root')
        if len(parts) > 2 and parts[1] in chrome and parts[-1] not in ('Text', 'Position', 'Width'):
            parts[0] = 'options'
            records.append(dict(path='.'.join(parts), value=record['value']))
    labels = {'pageTitle': 'PC Settings',
              'restartNotice': 'Changes apply the next time the game starts.',
              'XuiBackButton1': 'Back'}
    for ident, label, texts in ROWS:
        labels[ident + 'Label'] = label
        for n, text in enumerate(texts, 1):
            labels[f'{ident}.XuiRadioButton{n}'] = text
    for path, text in labels.items():
        edit.set_localized(records, 'options.'+path+'.Text', text)
    return xus.build(records)


def validate_package(data):
    entries = xzp.parse(data)
    if xzp.build(entries) != data:
        raise ValueError('package roundtrip mismatch')
    scenes = localizations = 0
    for entry in entries:
        if entry.name.lower().endswith('.xur'):
            scene = xur.parse(entry.data)
            if not scene.fully_parsed or scene.serialize() != entry.data:
                raise ValueError(f'incomplete or non-lossless {entry.name}: {scene.errors}')
            scenes += 1
        elif entry.name.lower().endswith('.xus'):
            if xus.build(xus.parse(entry.data)) != entry.data:
                raise ValueError(f'non-lossless {entry.name}')
            localizations += 1
    return dict(entries=len(entries), scenes=scenes, localizations=localizations)


def modify_package(original, standalone=False):
    entries = xzp.parse(original)
    modified, added = [], []
    found_menu = False
    for entry in entries:
        data = entry.data
        if entry.name == 'menuHud_3.xur' and standalone:
            data = modify_menu(data)
            found_menu = True
        elif entry.name == 'DisplayOptions.xur':
            added.append(xzp.Entry('options.xur', pc_scene(data)))
        elif entry.name == 'menuHud_3.xus':
            records = xus.parse(data)
            data = edit.set_localized(records, 'Menu.Menu.Tab1.pcSettings.Text', 'PC Settings')
            data = edit.set_localized(records, 'Menu.Menu.Tab1.pcSettings.PointSize', '-1.000000')
            data = edit.set_localized(records, 'Menu.Menu.Tab2.quitGame.Text', 'Quit to Desktop')
            data = edit.set_localized(records, 'Menu.Menu.Tab2.quitGame.PointSize', '-1.000000')
        elif entry.name == 'DisplayOptions.xus':
            added.append(xzp.Entry('options.xus', pc_localization(data)))
        modified.append(xzp.Entry(entry.name, data))
    existing_names = {e.name for e in entries}
    if any(e.name in existing_names for e in added) or len({e.name for e in added}) != len(added):
        raise ValueError('new scene/localization already exists or is ambiguous')
    if standalone and (not found_menu or added):
        raise ValueError('unexpected standalone menu package')
    if not standalone and not added:
        raise ValueError('package has no DisplayOptions scene/localization')
    return xzp.build(modified+added)


def run(game, manifest_text, output=REPO/'out/xui', manifest_source=MANIFEST_SOURCE):
    inputs = verify_inputs(game, manifest_records(manifest_text))
    output = xzp.output_root(output)
    protected = {(game.resolve()/rel).resolve() for rel in inputs}
    report = []
    for rel, original in inputs.items():
        print(f'Processing {rel}', flush=True)
        validate_package(original)
        result = modify_package(original, standalone=(rel == 'XUIFrontEnd/menuHUD_3.xzp'))
        parsed = validate_package(result)
        patch = xpatch.generate(original, result, {'options.xur': 'DisplayOptions.xur', 'options.xus': 'DisplayOptions.xus'})
        stats = xpatch.audit(original, patch)
        if stats['original_matching_literal_windows']:
            raise ValueError('patch literals contain original game spans')
        if xpatch.apply(original, patch) != result:
            raise ValueError('patch result mismatch')
        package_path = xzp.output_root(output/'modified/game'/rel)
        patch_rel = rel + '.xpatch'
        patch_path = xzp.output_root(output/'patches'/patch_rel)
        for target, data in ((package_path, result), (patch_path, patch)):
            if target in protected:
                raise ValueError('output would replace an input package')
            xzp.write_output(target, data)
        row = dict(path=rel, patch=patch_rel, original_sha256=hashlib.sha256(original).hexdigest(),
                   result_sha256=hashlib.sha256(result).hexdigest(), result_bytes=len(result), **parsed, **stats)
        report.append(row)
        print(f'  {stats["patch_bytes"]} patch bytes; {stats["literal_bytes"]} literal bytes; '
              f'{stats["original_matching_literal_windows"]} original literal windows >=16 bytes', flush=True)
    report_path = xzp.output_root(output/'patches/report.json')
    if report_path in protected:
        raise ValueError('report would replace an input package')
    xzp.write_output(report_path, (json.dumps(dict(manifest_source=manifest_source, english_labels_all_locales=True,
                                                packages=report), indent=2)+'\n').encode('utf-8'))
    return report


def modify_dlc_package(original):
    entries = xzp.parse(original)
    if sum(e.name == 'menuHud_3.xur' for e in entries) != 1:
        raise ValueError('unexpected DLC menu package')
    return xzp.build([xzp.Entry(e.name, modify_dlc_menu(e.data) if e.name == 'menuHud_3.xur' else e.data)
                      for e in entries])


def run_dlc(package, output=REPO/'out/xui'):
    """Scene_CommonDLC.xzp from an unpacked Free-For-All Pack -> Scene_CommonDLC.xzp.xpatch."""
    original = Path(package).read_bytes()
    if hashlib.sha256(original).hexdigest() != DLC_MENU_SHA256:
        raise ValueError('not the Free-For-All Pack Scene_CommonDLC.xzp (SHA256 mismatch)')
    validate_package(original)
    result = modify_dlc_package(original)
    parsed = validate_package(result)
    patch = xpatch.generate(original, result, {})
    stats = xpatch.audit(original, patch)
    if stats['original_matching_literal_windows']:
        raise ValueError('patch literals contain original game spans')
    if xpatch.apply(original, patch) != result:
        raise ValueError('patch result mismatch')
    patch_path = xzp.output_root(output/'patches/DLC/Scene_CommonDLC.xzp.xpatch')
    xzp.write_output(patch_path, patch)
    row = dict(path='DLC/XUIFrontEnd/Scene_CommonDLC.xzp', original_sha256=DLC_MENU_SHA256,
               result_sha256=hashlib.sha256(result).hexdigest(), result_bytes=len(result), **parsed, **stats)
    print(json.dumps(row, indent=2), flush=True)
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path, nargs='?')
    parser.add_argument('--manifest', type=Path, help='explicit trusted manifest override (same tab-separated format)')
    parser.add_argument('--dlc', type=Path, metavar='SCENE_COMMONDLC_XZP',
                        help='also patch the Free-For-All Pack XUIFrontEnd/Scene_CommonDLC.xzp (title update menu)')
    args = parser.parse_args()
    if args.dlc:
        run_dlc(args.dlc)
    if not args.game:
        if not args.dlc:
            parser.error('game folder or --dlc required')
        return
    text = args.manifest.read_text() if args.manifest else subprocess.run(
        ['git', 'show', MANIFEST_SOURCE], cwd=REPO, check=True, capture_output=True, text=True).stdout
    run(args.game, text, manifest_source=str(args.manifest) if args.manifest else MANIFEST_SOURCE)


if __name__ == '__main__':
    main()
