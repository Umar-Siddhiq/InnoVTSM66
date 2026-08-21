#!/usr/bin/env python3
"""Install physical copies of Project Guardian into common Agent Skills locations."""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

SKILL_NAME = "project-guardian-engineer"


def git_root(start: str | None = None) -> Path:
    cwd = Path(start or os.getcwd()).resolve()
    try:
        out = subprocess.check_output(["git", "-C", str(cwd), "rev-parse", "--show-toplevel"], stderr=subprocess.DEVNULL, text=True).strip()
        if out:
            return Path(out).resolve()
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    return cwd


def source_root() -> Path:
    return Path(__file__).resolve().parent.parent


def copy_skill(src: Path, dest: Path, force: bool) -> None:
    if dest.resolve() == src.resolve():
        print(f"Skip source itself: {dest}")
        return
    if dest.exists():
        if not force:
            raise FileExistsError(f"Destination exists: {dest}. Re-run with --force to replace this skill copy.")
        shutil.rmtree(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(src, dest, ignore=shutil.ignore_patterns("__pycache__", "*.pyc", ".DS_Store"))
    print(f"Installed: {dest}")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--repo")
    p.add_argument("--force", action="store_true")
    p.add_argument("--global", dest="global_install", action="store_true", help="Also install user-global copies for supported hosts.")
    p.add_argument("--bootstrap", action="store_true", help="Initialize repo memory/map and always-on AGENTS.md/CLAUDE.md blocks.")
    args = p.parse_args()
    src = source_root()
    root = git_root(args.repo)
    destinations = [
        root / ".agents" / "skills" / SKILL_NAME,
        root / ".claude" / "skills" / SKILL_NAME,
    ]
    if args.global_install:
        home = Path.home()
        destinations += [
            home / ".agents" / "skills" / SKILL_NAME,
            home / ".claude" / "skills" / SKILL_NAME,
            home / ".gemini" / "config" / "skills" / SKILL_NAME,
        ]
    try:
        for dest in destinations:
            copy_skill(src, dest, args.force)
    except FileExistsError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    if args.bootstrap:
        subprocess.check_call([sys.executable, str(src / "scripts" / "bootstrap_project.py"), "--repo", str(root), "--apply"])
    print("Cross-agent installation complete. Physical copies are used instead of symlinks for compatibility.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
