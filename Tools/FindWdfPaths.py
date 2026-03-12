#!/usr/bin/env python3
"""
Find likely WDF path prefixes by matching hashed UIDs.
"""

import argparse
from pathlib import Path
from UnpackWdf import WdfReader, stringtoid, normalize_path


def try_path_prefixes(wdf_path: str, data_dir: str):
    reader = WdfReader(wdf_path)
    entries = reader.read_index()
    uid_set = {entry.uid for entry in entries}

    data_root = Path(data_dir)
    all_files = []
    for file_path in data_root.rglob('*'):
        if file_path.is_file():
            all_files.append(str(file_path.relative_to(data_root)).replace('\\', '/'))

    print(f'WDF Archive: {wdf_path}')
    print(f'Data Folder: {data_dir}')
    print(f'WDF Entries: {len(entries)}')
    print(f'Files Found: {len(all_files)}')
    print()

    prefixes = ['', 'data/', 'Data/', 'c3/', 'c3/data/', './', './data/', 'map/', 'data/map/']

    best_prefix = None
    best_matches = 0

    for prefix in prefixes:
        matches = 0
        for file_path in all_files:
            test_path = prefix + file_path
            uid = stringtoid(normalize_path(test_path))
            if uid in uid_set:
                matches += 1

        if matches > 0:
            print(f"  Prefix: '{prefix}' -> {matches} matches")
            if matches > best_matches:
                best_matches = matches
                best_prefix = prefix

    print('\n' + '=' * 60)
    if best_prefix is None:
        print('No matches found with common prefixes.')
        return

    print(f"BEST PREFIX: '{best_prefix}' with {best_matches} matches")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('archive')
    parser.add_argument('data_folder')
    args = parser.parse_args()
    try_path_prefixes(args.archive, args.data_folder)


if __name__ == '__main__':
    main()