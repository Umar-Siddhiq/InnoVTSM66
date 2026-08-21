#!/usr/bin/env python3
"""Check intended or changed paths against Project Guardian protection rules."""
from __future__ import annotations

import argparse
import fnmatch
import json
import os
import subprocess
from pathlib import Path
from typing import Any


def git_root(start: str | None = None) -> Path:
    cwd = Path(start or os.getcwd()).resolve()
    try:
        out = subprocess.check_output(
            ["git", "-C", str(cwd), "rev-parse", "--show-toplevel"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
        if out:
            return Path(out).resolve()
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    return cwd


def load_rules(root: Path) -> list[dict[str, Any]]:
    path = root / ".ai" / "PROTECTED_AREAS.json"
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError):
        return []
    return [x for x in data.get("areas", []) if isinstance(x, dict) and x.get("pattern")]


def normalize(path: str, root: Path) -> str:
    p = Path(path)
    if p.is_absolute():
        try:
            p = p.resolve().relative_to(root)
        except ValueError:
            return p.as_posix()
    return p.as_posix().lstrip("./")


def matches(path: str, pattern: str) -> bool:
    p = path.lower().replace("\\", "/")
    pat = pattern.lower().replace("\\", "/")
    if fnmatch.fnmatchcase(p, pat):
        return True
    if pat.startswith("**/") and fnmatch.fnmatchcase(p, pat[3:]):
        return True
    return False


def changed_paths(root: Path) -> list[str]:
    commands = [
        ["git", "-C", str(root), "diff", "--name-only"],
        ["git", "-C", str(root), "diff", "--cached", "--name-only"],
        ["git", "-C", str(root), "ls-files", "--others", "--exclude-standard"],
    ]
    found: set[str] = set()
    for cmd in commands:
        try:
            out = subprocess.check_output(cmd, stderr=subprocess.DEVNULL, text=True)
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue
        found.update(line.strip().replace("\\", "/") for line in out.splitlines() if line.strip())
    return sorted(found)


def evaluate(paths: list[str], rules: list[dict[str, Any]], allow: bool, reason: str | None) -> int:
    blocked: list[tuple[str, dict[str, Any]]] = []
    warnings: list[tuple[str, dict[str, Any]]] = []
    for path in paths:
        for rule in rules:
            if matches(path, str(rule["pattern"])):
                if rule.get("level", "hard") == "watch":
                    warnings.append((path, rule))
                else:
                    blocked.append((path, rule))
    for path, rule in warnings:
        print(f"WATCH: {path} matches {rule['pattern']}: {rule.get('reason', '')}")
    if blocked:
        for path, rule in blocked:
            print(f"BLOCK: {path} matches {rule['pattern']} [{rule.get('level', 'hard')}]: {rule.get('reason', '')}")
        if allow and reason and reason.strip():
            print(f"OVERRIDE: protected edit allowed with recorded reason: {reason.strip()}")
            return 0
        print("Protected edit rejected. An explicit override reason is required only after the user explicitly authorizes this protected change.")
        return 2
    print(f"Guardrail OK: checked {len(paths)} path(s); no blocking protection matched.")
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--repo")
    p.add_argument("--allow-protected", action="store_true", help="Mechanically allow a protected edit after explicit user authorization.")
    p.add_argument("--reason", help="Required when --allow-protected is used on a blocking rule.")
    sub = p.add_subparsers(dest="command", required=True)
    s = sub.add_parser("check-paths")
    s.add_argument("paths", nargs="+")
    sub.add_parser("check-diff")
    return p


def main() -> int:
    args = build_parser().parse_args()
    root = git_root(args.repo)
    rules = load_rules(root)
    if args.command == "check-diff":
        paths = changed_paths(root)
    else:
        paths = [normalize(x, root) for x in args.paths]
    return evaluate(paths, rules, args.allow_protected, args.reason)


if __name__ == "__main__":
    raise SystemExit(main())
