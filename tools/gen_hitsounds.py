"""Build embedded hitsound pack from ar4yc + neverlose extras.

Stores binary WAVs under hitsounds_bin/ and generates a .rc resource script
plus a small C++ table. Data is linked into the exe (not external at runtime).
"""
import json
import re
import shutil
import urllib.request
from pathlib import Path

root = Path(__file__).resolve().parents[1]
misc = root / "Cheat" / "Core" / "Features" / "Misc"
tmp = misc / "_hitsounds_tmp"
bin_dir = misc / "hitsounds_bin"
tmp.mkdir(parents=True, exist_ok=True)

api_path = Path(
    r"C:\Users\1\.cursor\projects\c-Users-1-Desktop-new-base-jewsploit"
    r"\agent-tools\9a384e87-6ac5-4f89-a322-a8390fd47d53.txt"
)
api = json.loads(api_path.read_text(encoding="utf-8"))
ar4 = [e for e in api if e.get("name", "").endswith(".wav") and e.get("download_url")]

neverlose = [
    ("ben", "https://raw.githubusercontent.com/Kittywy/Neverlose.cc-hitsounds/main/wav/ben.wav"),
    ("spiral knight", "https://raw.githubusercontent.com/Kittywy/Neverlose.cc-hitsounds/main/wav/spiral%20knight.wav"),
    ("tavern misc1c", "https://raw.githubusercontent.com/Kittywy/Neverlose.cc-hitsounds/main/wav/tavern%20misc1c.wav"),
    ("ting", "https://raw.githubusercontent.com/Kittywy/Neverlose.cc-hitsounds/main/wav/ting.wav"),
]

RID_BASE = 31000

def norm(name: str) -> str:
    n = name.lower()
    n = re.sub(r"\.wav$", "", n)
    n = n.replace("_", " ").replace("-", " ")
    n = re.sub(r"\s+", " ", n).strip()
    return n

entries: dict[str, tuple[str, Path]] = {}

def add(display: str, path: Path) -> bool:
    key = norm(display)
    if key in entries:
        return False
    entries[key] = (display, path)
    return True

print(f"downloading {len(ar4)} ar4yc...")
for i, e in enumerate(ar4):
    name = e["name"]
    display = Path(name).stem.replace("_", " ")
    dest = tmp / name
    if not dest.exists() or dest.stat().st_size != e["size"]:
        print(f"  get {name}")
        urllib.request.urlretrieve(e["download_url"], dest)
    add(display, dest)
    if (i + 1) % 25 == 0:
        print(f"  {i + 1}/{len(ar4)}")

print("downloading neverlose extras...")
for display, url in neverlose:
    dest = tmp / f"nl_{display}.wav"
    if not dest.exists():
        try:
            urllib.request.urlretrieve(url, dest)
        except Exception as ex:
            print("fail", display, ex)
            continue
    add(display, dest)

items = sorted(entries.values(), key=lambda t: t[0].lower())
print("unique", len(items), "mb", round(sum(p.stat().st_size for _, p in items) / 1024 / 1024, 2))

if bin_dir.exists():
    shutil.rmtree(bin_dir)
bin_dir.mkdir(parents=True)

for p in misc.glob("hitsounds_data_*.cpp"):
    p.unlink()
for p in (misc / "hitsounds_data_table.cpp", misc / "hitsounds.rc", misc / "hitsounds_res.h"):
    if p.exists():
        p.unlink()

packed: list[tuple[str, int, str]] = []
for i, (display, src) in enumerate(items):
    rid = RID_BASE + i
    safe = re.sub(r"[^A-Za-z0-9]+", "_", display).strip("_")[:48] or f"sound_{i}"
    fname = f"{i:03d}_{safe}.wav"
    dst = bin_dir / fname
    shutil.copy2(src, dst)
    packed.append((display, rid, f"hitsounds_bin\\\\{fname}"))

res_h = [
    "#pragma once",
    "",
    f"#define IDR_HITSOUND_COUNT {len(packed)}",
    f"#define IDR_HITSOUND_FIRST {RID_BASE}",
    "",
]
for display, rid, _ in packed:
    sym = re.sub(r"[^A-Za-z0-9]", "_", display).upper()
    sym = re.sub(r"_+", "_", sym).strip("_")
    if sym[0].isdigit():
        sym = "HS_" + sym
    res_h.append(f"#define IDR_HS_{sym[:40]} {rid}")
(misc / "hitsounds_res.h").write_text("\n".join(res_h) + "\n", encoding="utf-8")

rc = [
    "#include \"hitsounds_res.h\"",
    "",
]
for display, rid, rel in packed:
    rc.append(f"{rid} RCDATA \"{rel}\"")
(misc / "hitsounds.rc").write_text("\n".join(rc) + "\n", encoding="utf-8")

cpp = [
    '#include "hitsounds_data.h"',
    '#include "hitsounds_res.h"',
    "",
    "#include <Windows.h>",
    "",
    "#include <vector>",
    "",
    "namespace Cheat::Features::HitSoundsData {",
    "namespace {",
    "",
    "struct ResEntry {",
    "    const char* name;",
    "    int rid;",
    "};",
    "",
    "const ResEntry k_res[] = {",
]
for display, rid, _ in packed:
    esc = display.replace("\\", "\\\\").replace('"', '\\"')
    cpp.append(f'    {{ "{esc}", {rid} }},')
cpp += [
    "};",
    "",
    f"constexpr int k_res_count = {len(packed)};",
    "",
    "struct Loaded {",
    "    const unsigned char* data;",
    "    std::size_t size;",
    "};",
    "",
    "Loaded g_loaded[k_res_count]{};",
    "bool g_ready = false;",
    "",
    "void EnsureLoaded()",
    "{",
    "    if (g_ready) return;",
    "    HMODULE mod = GetModuleHandleW(nullptr);",
    "    for (int i = 0; i < k_res_count; ++i) {",
    "        HRSRC hrsrc = FindResourceW(mod, MAKEINTRESOURCEW(k_res[i].rid), RT_RCDATA);",
    "        if (!hrsrc) continue;",
    "        HGLOBAL hglob = LoadResource(mod, hrsrc);",
    "        if (!hglob) continue;",
    "        void* ptr = LockResource(hglob);",
    "        const DWORD sz = SizeofResource(mod, hrsrc);",
    "        if (!ptr || sz == 0) continue;",
    "        g_loaded[i].data = static_cast<const unsigned char*>(ptr);",
    "        g_loaded[i].size = static_cast<std::size_t>(sz);",
    "    }",
    "    g_ready = true;",
    "}",
    "",
    "}",
    "",
    "static Entry g_entries[k_res_count]{};",
    "static bool g_entries_ready = false;",
    "",
    "const Entry* Entries()",
    "{",
    "    EnsureLoaded();",
    "    if (!g_entries_ready) {",
    "        for (int i = 0; i < k_res_count; ++i) {",
    "            g_entries[i].name = k_res[i].name;",
    "            g_entries[i].data = g_loaded[i].data;",
    "            g_entries[i].size = g_loaded[i].size;",
    "        }",
    "        g_entries_ready = true;",
    "    }",
    "    return g_entries;",
    "}",
    "",
    "const Entry* k_entries = nullptr;",
    "",
    "int Count()",
    "{",
    "    return k_res_count;",
    "}",
    "",
    "const Entry& At(int index)",
    "{",
    "    const Entry* e = Entries();",
    "    return e[index];",
    "}",
    "",
    "}",
    "",
]
(misc / "hitsounds_data.cpp").write_text("\n".join(cpp), encoding="utf-8")

header = "\n".join(
    [
        "#pragma once",
        "#include <cstddef>",
        "",
        "namespace Cheat::Features::HitSoundsData {",
        "",
        "struct Entry {",
        "    const char* name;",
        "    const unsigned char* data;",
        "    std::size_t size;",
        "};",
        "",
        "int Count();",
        "const Entry& At(int index);",
        "const Entry* Entries();",
        "",
        "}",
        "",
    ]
)
(misc / "hitsounds_data.h").write_text(header, encoding="utf-8")

print("wrote", len(packed), "binaries + rc + cpp")
print("done")
