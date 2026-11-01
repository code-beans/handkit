#!/usr/bin/env python3
"""Convert ArkOS EmulationStation's configured physical buttons to SDL mappings.

Run before SDL_Init; use the output as SDL_GAMECONTROLLERCONFIG_FILE.
An explicit gamecontrollerdb.txt may be used instead on unusual firmware.
"""
import re
import sys
import xml.etree.ElementTree as ET

# ES uses Nintendo labels; SDL uses Xbox positions. Do not map hotkeyenable:
# it usually duplicates Select and would turn it into two SDL buttons.
BUTTONS = {
    'a': 'b', 'b': 'a', 'x': 'y', 'y': 'x',
    'start': 'start', 'select': 'back',
    'leftshoulder': 'leftshoulder', 'rightshoulder': 'rightshoulder',
    'lefttrigger': 'lefttrigger', 'righttrigger': 'righttrigger',
    'leftthumb': 'leftstick', 'rightthumb': 'rightstick',
    'up': 'dpup', 'down': 'dpdown', 'left': 'dpleft', 'right': 'dpright',
    'leftanalogup': 'lefty', 'leftanalogleft': 'leftx',
    'rightanalogup': 'righty', 'rightanalogleft': 'rightx',
}


def convert(path):
    for pad in ET.parse(path).getroot().findall('inputConfig'):
        guid = pad.get('deviceGUID', '')
        if pad.get('type') != 'joystick' or not re.fullmatch('[0-9a-fA-F]{32}', guid):
            continue
        fields = []
        for item in pad.findall('input'):
            name = item.get('name')
            target = BUTTONS.get(name)
            if not target:
                continue
            index, value = int(item.get('id')), int(item.get('value'))
            kind = item.get('type')
            if index < 0:
                continue
            if kind == 'button':
                binding = 'b%d' % index
            elif kind == 'hat':
                binding = 'h%d.%d' % (index, value)
            elif kind == 'axis':
                if 'analog' in name:
                    binding = 'a%d%s' % (index, '~' if value > 0 else '')
                else:
                    binding = '%sa%d' % ('+' if value > 0 else '-', index)
            else:
                continue
            fields.append('%s:%s' % (target, binding))
        if fields:
            name = pad.get('deviceName', 'ArkOS controller').replace(',', ' ').replace('\n', ' ')
            yield ','.join([guid, name] + fields + ['platform:Linux', ''])


if __name__ == '__main__':
    try:
        print('\n'.join(convert(sys.argv[1])))
    except (OSError, ValueError, TypeError, ET.ParseError, IndexError) as error:
        print('Cannot read ArkOS controller configuration: %s' % error, file=sys.stderr)
        sys.exit(1)
