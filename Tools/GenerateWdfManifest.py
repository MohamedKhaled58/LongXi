#!/usr/bin/env python3
"""
Generate a WDF manifest by matching real files against WDF UIDs.
"""

import argparse
from pathlib import Path
from UnpackWdf import WdfReader, stringtoid, normalize_path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('archive')
    parser.add_argument('data_folder')
    parser.add_argument('--output', default=None)
    args = parser.parse_args()

    reader = WdfReader(args.archive)
    entries = reader.read_index()
    uid_set = {e.uid for e in entries}

    root = Path(args.data_folder)
    files = [p for p in root.rglob('*') if p.is_file()]

    matched = []
    for file_path in files:
        rel = normalize_path(str(file_path.relative_to(root)).replace('\\', '/'))
        uid = stringtoid(rel)
        if uid in uid_set:
            matched.append(rel)

    output = args.output or (Path(args.archive).stem + '_manifest.txt')
    Path(output).write_text('\n'.join(sorted(set(matched))), encoding='utf-8')

    print(f'Matched {len(matched)} files')
    print(f'Wrote manifest: {output}')


if __name__ == '__main__':
    main()