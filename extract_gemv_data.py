#!/usr/bin/env python3
"""Extract a ZIP containing GEMV data into PIMSimulator/data/."""
import argparse
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import tempfile
import zipfile


def choose_archive(root, supplied):
    if supplied:
        p = Path(supplied).expanduser().resolve()
        if not p.is_file():
            raise FileNotFoundError(f"ZIP file not found: {p}")
        return p
    names = ("data.zip", "Data.zip", "GEMV_data.zip", "gemv_data.zip")
    for name in names:
        p = root / name
        if p.is_file():
            return p
    matches = sorted(p for p in root.glob("*.zip") if "data" in p.name.lower())
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        raise ValueError("Multiple data ZIPs found. Pass the filename explicitly: python3 extract_gemv_data.py <archive.zip>")
    raise FileNotFoundError("No data ZIP found in the project directory. Place data.zip next to this script, or pass its path.")


def main():
    parser = argparse.ArgumentParser(description="Unpack the GEMV dataset archive into PIMSimulator/data/.")
    parser.add_argument("archive", nargs="?", help="ZIP path (default: data.zip in this directory)")
    parser.add_argument("--overwrite", action="store_true", help="Replace existing extracted files")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    archive = choose_archive(root, args.archive)
    dest = root / "data"
    if archive.resolve().is_relative_to(dest.resolve()):
        raise ValueError("Keep the ZIP in the project root, not inside data/.")

    with zipfile.ZipFile(archive) as zf:
        files = [info for info in zf.infolist() if not info.is_dir()]
        if not files:
            raise ValueError("The ZIP has no files.")
        names = [PurePosixPath(info.filename.replace('\\', '/')) for info in files]
        # A ZIP made by compressing the data/ folder often starts with data/;
        # some archives also contain a surrounding project folder.
        components = [p.parts for p in names]
        positions = [next((i for i, s in enumerate(parts) if s.lower() == "data"), -1) for parts in components]
        if all(i >= 0 for i in positions):
            relative = [PurePosixPath(*parts[i + 1:]) for parts, i in zip(components, positions)]
        else:
            # Archives made from INSIDE data/ can be extracted directly.
            # Also remove a single shared wrapper directory, if present.
            common_wrapper = (all(len(parts) > 1 for parts in components)
                              and len({parts[0] for parts in components}) == 1)
            relative = [PurePosixPath(*parts[1:] if common_wrapper else parts)
                        for parts in components]

        entries = []
        for info, rel in zip(files, relative):
            if not rel.parts or rel.is_absolute() or any(part in ("..", "") for part in rel.parts):
                raise ValueError(f"Unsafe or empty path in ZIP: {info.filename}")
            if info.filename.startswith('/') or '..' in PurePosixPath(info.filename.replace('\\', '/')).parts:
                raise ValueError(f"Unsafe source path in ZIP: {info.filename}")
            mode = info.external_attr >> 16
            if stat.S_ISLNK(mode):
                raise ValueError(f"ZIP contains a symlink, which is not supported: {info.filename}")
            entries.append((info, rel))
        if len({str(rel) for _, rel in entries}) != len(entries):
            raise ValueError("Different ZIP paths map to the same output filename.")

        total = sum(info.file_size for info, _ in entries)
        free = shutil.disk_usage(root).free
        if total > free:
            raise OSError(f"Not enough free disk space: archive expands to {total / 1024**3:.2f} GiB, "
                          f"only {free / 1024**3:.2f} GiB free.")
        print(f"Archive: {archive.name}")
        print(f"Destination: {dest}")
        print(f"Files in archive: {len(entries)} | Uncompressed size: {total / 1024**3:.2f} GiB")
        dest.mkdir(parents=True, exist_ok=True)
        extracted = skipped = 0
        for info, rel in entries:
            target = dest.joinpath(*rel.parts)
            if target.exists() and not args.overwrite:
                skipped += 1
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            # Avoid partial files if a ZIP entry is corrupt/interrupted.
            fd, temp = tempfile.mkstemp(prefix='.gemv_extract_', dir=target.parent)
            try:
                with os.fdopen(fd, 'wb') as output, zf.open(info) as source:
                    shutil.copyfileobj(source, output, length=1024 * 1024)
                os.replace(temp, target)
            finally:
                if os.path.exists(temp):
                    os.unlink(temp)
            extracted += 1
        print(f"Finished: extracted {extracted} files, skipped {skipped} existing files.")
        print("Data ready in PIMSimulator/data/.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)
