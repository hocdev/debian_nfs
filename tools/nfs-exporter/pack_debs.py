import lzma
import os
import sys
from pathlib import Path

def ar_header(name: str, size: int) -> bytes:
    # Standard Debian ar header: 60 bytes ASCII
    name_field = name.encode("ascii")[:16].ljust(16, b" ")
    mtime = b"1700000000  "
    uid = b"0     "
    gid = b"0     "
    mode = b"100644  "
    size_field = f"{size:<10}".encode("ascii")
    end = b"`\n"
    hdr = name_field + mtime + uid + gid + mode + size_field + end
    assert len(hdr) == 60
    return hdr

def pack_deb(pkg_dir: Path, out_deb: Path) -> None:
    ctrl_tar = (pkg_dir / "control.tar").read_bytes()
    data_tar = (pkg_dir / "data.tar").read_bytes()
    ctrl_xz = lzma.compress(ctrl_tar, preset=6)
    data_xz = lzma.compress(data_tar, preset=6)
    deb_bin = b"2.0\n"

    with out_deb.open("wb") as f:
        f.write(b"!<arch>\n")
        for name, payload in [
            ("debian-binary", deb_bin),
            ("control.tar.xz", ctrl_xz),
            ("data.tar.xz", data_xz),
        ]:
            f.write(ar_header(name, len(payload)))
            f.write(payload)
            if len(payload) % 2 == 1:
                f.write(b"\n")

def main() -> None:
    if len(sys.argv) != 3:
        print("Usage: pack_debs.py <staging_dir> <repo_pool_dir>")
        sys.exit(1)
    staging = Path(sys.argv[1])
    pool = Path(sys.argv[2])
    pool.mkdir(parents=True, exist_ok=True)
    for d in sorted(staging.iterdir()):
        if d.is_dir() and (d / "control.tar").exists():
            out = pool / f"{d.name}.deb"
            pack_deb(d, out)
            mb = out.stat().st_size / (1024 * 1024)
            print(f"Packed {out.name}: {mb:.2f} MB")

if __name__ == "__main__":
    main()
