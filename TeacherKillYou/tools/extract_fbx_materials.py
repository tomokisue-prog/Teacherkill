"""Keep embedded GLB materials/images without duplicating the FBX geometry.

Usage: python tools/extract_fbx_materials.py <source.glb or directory>
The original files are never modified. The game prefers <stem>.materials.glb.
"""
import argparse
import copy
import json
import os
from pathlib import Path
import struct
import tempfile


def extract(source, force=False):
    output = source.with_name(source.stem + ".materials.glb")
    with source.open("rb") as file:
        header = file.read(20)
        if len(header) != 20:
            raise ValueError("Truncated GLB header")
        magic, version, total, json_size, json_type = struct.unpack("<5I", header)
        if (magic, version, json_type) != (0x46546C67, 2, 0x4E4F534A) or total != source.stat().st_size:
            raise ValueError("Expected a complete GLB 2.0 file")
        document = json.loads(file.read(json_size))
        bin_size, bin_type = struct.unpack("<2I", file.read(8))
        bin_start = file.tell()
        if bin_type != 0x004E4942 or bin_start + bin_size != total:
            raise ValueError("Expected one embedded BIN chunk")
        if not document.get("materials"):
            raise ValueError("No materials to extract")

        result = {key: copy.deepcopy(document[key]) for key in ("asset", "materials", "textures", "samplers") if key in document}
        images, views, binary = [], [], bytearray()
        for image in document.get("images", []):
            if "bufferView" not in image or "uri" in image:
                raise ValueError("Only embedded images are supported")
            view = document["bufferViews"][image["bufferView"]]
            offset, size = view.get("byteOffset", 0), view["byteLength"]
            if view.get("buffer", 0) != 0 or offset < 0 or size <= 0 or offset + size > bin_size:
                raise ValueError("Invalid embedded image range")
            file.seek(bin_start + offset)
            content = file.read(size)
            if len(content) != size:
                raise ValueError("Truncated image")
            binary.extend(b"\0" * (-len(binary) % 4))
            views.append({"buffer": 0, "byteOffset": len(binary), "byteLength": size})
            binary.extend(content)
            item = copy.deepcopy(image)
            item["bufferView"] = len(views) - 1
            images.append(item)

    if not images:
        raise ValueError("No embedded material images")
    # Keep only extension declarations used by the retained material data.
    used = set()
    def find_extensions(value):
        if isinstance(value, dict):
            used.update(value.get("extensions", {}).keys())
            for child in value.values():
                find_extensions(child)
        elif isinstance(value, list):
            for child in value:
                find_extensions(child)
    result["images"] = images
    find_extensions(result)
    for key in ("extensionsUsed", "extensionsRequired"):
        extensions = [name for name in document.get(key, []) if name in used]
        if extensions:
            result[key] = extensions
    binary.extend(b"\0" * (-len(binary) % 4))
    result["buffers"] = [{"byteLength": len(binary)}]
    result["bufferViews"] = views
    metadata = json.dumps(result, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    metadata += b" " * (-len(metadata) % 4)
    payload = (struct.pack("<5I", 0x46546C67, 2, 28 + len(metadata) + len(binary), len(metadata), 0x4E4F534A)
               + metadata + struct.pack("<2I", len(binary), 0x004E4942) + binary)
    if output.exists() and output.read_bytes() != payload and not force:
        raise ValueError(f"{output} already exists with different content; use --force to regenerate")
    if not output.exists() or output.read_bytes() != payload:
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=output.parent, suffix=".tmp", delete=False) as file:
                temporary = Path(file.name)
                file.write(payload)
            os.replace(temporary, output)
        finally:
            if temporary is not None and temporary.exists():
                temporary.unlink()
    print(f"{source.name}: {source.stat().st_size:,} -> {output.name}: {len(payload):,} bytes (source preserved)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--force", action="store_true", help="replace an existing generated companion")
    args = parser.parse_args()
    files = sorted(args.source.glob("*.glb")) if args.source.is_dir() else [args.source]
    files = [path for path in files if not path.name.endswith(".materials.glb")]
    if not files:
        parser.error("no source GLB files found")
    for path in files:
        try:
            extract(path, args.force)
        except (OSError, ValueError, KeyError, IndexError, struct.error) as error:
            parser.exit(1, f"{path}: {error}\n")


if __name__ == "__main__":
    main()
