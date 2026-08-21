#!/usr/bin/env python3
"""Bootstrap repository-local Project Guardian memory, mapping, and always-on rules."""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any

START = "<!-- PROJECT-GUARDIAN:START -->"
END = "<!-- PROJECT-GUARDIAN:END -->"
BLOCK = f"""{START}
## Project Guardian (always-on)

1. Read `.ai/CRITICAL_INVARIANTS.md`, `.ai/MEMORY_INDEX.md`, `.ai/PROTECTED_AREAS.json`, and `.ai/PROJECT_MAP.md` before engineering changes.
2. Search project memory for task/component terms before touching related code.
3. Treat `why`, `check`, `analyze`, `review`, `inspect`, and `find issue` as read-only; do not edit unless the user explicitly requests a change.
4. Never modify a protected area incidentally. Run the Project Guardian guardrail before protected/high-risk edits.
5. Diagnose from evidence before implementation; make the smallest complete change and preserve unrelated work.
6. After a validated fix, correction, discovered invariant, or high-value failed attempt, write project memory automatically. Do not wait for the user to ask.
7. A meaningful implementation is not complete until validation, final diff review, protected-area checks, and memory write-back pass.
8. Use the `project-guardian-engineer` skill for the detailed workflow when the host supports Agent Skills.
{END}
"""
DEFAULT_PROTECTIONS = [
    ("**/*gps*", "high-risk", "GPS/GNSS behavior is regression-sensitive. Diagnose first and require explicit scope before changing it."),
    ("**/*gnss*", "high-risk", "GNSS enable/power/init/parser behavior is regression-sensitive."),
    ("**/*modem*", "high-risk", "Modem registration/profile/state logic can cause field regressions."),
    ("**/*bootloader*", "high-risk", "Bootloader changes can brick devices or alter upgrade behavior."),
    ("**/*ota*", "high-risk", "OTA/FOTA changes can affect device recovery and production updates."),
    ("**/*fota*", "high-risk", "FOTA changes can affect device recovery and production updates."),
]


def git_root(start: str | None = None) -> Path:
    cwd = Path(start or os.getcwd()).resolve()
    try:
        out = subprocess.check_output(["git", "-C", str(cwd), "rev-parse", "--show-toplevel"], stderr=subprocess.DEVNULL, text=True).strip()
        if out:
            return Path(out).resolve()
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    return cwd


def script(name: str) -> Path:
    return Path(__file__).resolve().parent / name


def managed_write(path: Path) -> None:
    if path.exists():
        original = path.read_text(encoding="utf-8", errors="ignore")
        if START in original and END in original:
            before, rest = original.split(START, 1)
            _, after = rest.split(END, 1)
            merged = before.rstrip() + "\n\n" + BLOCK + after.lstrip("\n")
        else:
            merged = original.rstrip() + "\n\n" + BLOCK
    else:
        merged = f"# {path.stem} Project Instructions\n\n" + BLOCK
    path.write_text(merged.rstrip() + "\n", encoding="utf-8")


def load_json(path: Path, default: Any) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError):
        return default


def add_default_protections(root: Path) -> None:
    path = root / ".ai" / "PROTECTED_AREAS.json"
    data = load_json(path, {"version": 1, "areas": []})
    areas = data.setdefault("areas", [])
    existing = {x.get("pattern") for x in areas if isinstance(x, dict)}
    for pattern, level, reason in DEFAULT_PROTECTIONS:
        if pattern not in existing:
            areas.append({"pattern": pattern, "level": level, "reason": reason, "source": "project-guardian-default"})
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run(cmd: list[str]) -> None:
    subprocess.check_call(cmd)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--repo")
    p.add_argument("--apply", action="store_true", help="Also add/update compact managed blocks in AGENTS.md and CLAUDE.md.")
    p.add_argument("--no-default-protections", action="store_true")
    args = p.parse_args()
    root = git_root(args.repo)
    run([sys.executable, str(script("memory_store.py")), "--repo", str(root), "init"])
    if not args.no_default_protections:
        add_default_protections(root)
    run([sys.executable, str(script("project_map.py")), "--repo", str(root)])
    if args.apply:
        managed_write(root / "AGENTS.md")
        managed_write(root / "CLAUDE.md")
        print("Updated compact Project Guardian managed blocks in AGENTS.md and CLAUDE.md.")
    else:
        print("Dry-safe bootstrap complete. Use --apply to add always-on managed blocks without replacing existing instructions.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
