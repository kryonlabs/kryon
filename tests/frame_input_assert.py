"""Assertions for source/saved-IR keyboard, IME and clipboard replay."""
import json
from pathlib import Path
import sys

build = Path(sys.argv[1])
record = json.loads((build / 'source.json').read_text())
assert record['version'] == 2
frames = record['frames']
assert [frame['result'] for frame in frames] == [1, 4, 4, 4, 4, 7, 7, 7, 5, 4]
assert frames[4]['input']['ime_text'] == '👨‍👩‍👧‍👦'
assert frames[7]['clipboard_output'] == 'A界é'
assert frames[8]['input']['width'] == 640 and frames[8]['input']['height'] == 320
assert frames[8]['input']['wheel'] == 1
assert frames[2]['capture']['nodes'][1]['text_cursor'] == 1
assert frames[6]['capture']['nodes'][1]['text_value'] == 'A界é'
frames[3]['capture']['nodes'][1]['label'] = 'Changed label'
(build / 'changed.json').write_text(json.dumps(record, ensure_ascii=False))
(build / 'failure.trace').write_text('0 0 0 0 0 0 0 0 0 0 0 13 160 0 - 0 0 0 - -\n')
(build / 'invalid.trace').write_text('0 0 0 0 0 0 0 0 0 0 0 320 160 0 ff 0 0 0 - -\n')
