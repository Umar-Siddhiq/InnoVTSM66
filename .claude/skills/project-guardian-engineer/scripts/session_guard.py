#!/usr/bin/env python3
"""Capture a project baseline and enforce Project Guardian completion gates."""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import subprocess
import sys
import uuid
from pathlib import Path
from typing import Any

HARNESS_PREFIXES = (".ai/", ".agents/skills/project-guardian-engineer/", ".claude/skills/project-guardian-engineer/")
HARNESS_FILES = {"AGENTS.md", "CLAUDE.md"}


def now_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()


def git_root(start: str | None = None) -> Path:
    cwd = Path(start or os.getcwd()).resolve()
    try:
        out = subprocess.check_output(["git", "-C", str(cwd), "rev-parse", "--show-toplevel"], stderr=subprocess.DEVNULL, text=True).strip()
        if out:
            return Path(out).resolve()
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    return cwd


def git_lines(root: Path, args: list[str]) -> list[str]:
    try:
        out = subprocess.check_output(["git", "-C", str(root), *args], stderr=subprocess.DEVNULL, text=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        return []
    return [x.strip().replace("\\", "/") for x in out.splitlines() if x.strip()]


def dirty_paths(root: Path) -> set[str]:
    paths: set[str] = set()
    paths.update(git_lines(root, ["diff", "--name-only"]))
    paths.update(git_lines(root, ["diff", "--cached", "--name-only"]))
    paths.update(git_lines(root, ["ls-files", "--others", "--exclude-standard"]))
    return paths


def hash_path(root: Path, rel: str) -> str:
    path = root / rel
    if not path.exists():
        return "<deleted>"
    if path.is_dir():
        return "<dir>"
    h = hashlib.sha256()
    try:
        with path.open("rb") as fh:
            while True:
                chunk = fh.read(1024 * 1024)
                if not chunk:
                    break
                h.update(chunk)
        return h.hexdigest()
    except OSError:
        return "<unreadable>"


def snapshot(root: Path) -> dict[str, str]:
    return {p: hash_path(root, p) for p in sorted(dirty_paths(root))}


def read_json(path: Path, default: Any) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError):
        return default


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def session_path(root: Path) -> Path:
    return root / ".ai" / "session" / "current.json"


def skill_script(name: str) -> Path:
    return Path(__file__).resolve().parent / name


def cmd_start(args: argparse.Namespace) -> int:
    root = git_root(args.repo)
    path = session_path(root)
    existing = read_json(path, {})
    if existing.get("active") and not args.force:
        print(f"Active guardian session already exists: {existing.get('session_id')}. Use --force only if the prior session is abandoned.")
        return 2
    data = {
        "version": 1,
        "active": True,
        "session_id": uuid.uuid4().hex[:16],
        "started_at": now_iso(),
        "head": (git_lines(root, ["rev-parse", "--short", "HEAD"]) or [""])[0],
        "baseline_dirty_hashes": snapshot(root),
    }
    write_json(path, data)
    print(f"Started guardian session {data['session_id']} with {len(data['baseline_dirty_hashes'])} pre-existing dirty path(s).")
    return 0


def changed_since_start(root: Path, session: dict[str, Any]) -> list[str]:
    baseline = session.get("baseline_dirty_hashes", {}) if isinstance(session.get("baseline_dirty_hashes"), dict) else {}
    current = snapshot(root)
    result = []
    for path, digest in current.items():
        if path not in baseline or baseline.get(path) != digest:
            result.append(path)
    return sorted(result)


def is_harness_only(path: str) -> bool:
    return path in HARNESS_FILES or any(path.startswith(prefix) for prefix in HARNESS_PREFIXES)


def has_session_memory(root: Path, session_id: str) -> bool:
    path = root / ".ai" / "memory" / "events.jsonl"
    if not path.exists():
        return False
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        try:
            item = json.loads(line)
        except json.JSONDecodeError:
            continue
        if item.get("session_id") == session_id:
            return True
    return False


def run_guardrail(root: Path, paths: list[str], allow: bool, reason: str | None) -> int:
    if not paths:
        return 0
    cmd = [sys.executable, str(skill_script("guardrail.py")), "--repo", str(root)]
    if allow:
        cmd.append("--allow-protected")
        if reason:
            cmd += ["--reason", reason]
    cmd += ["check-paths", *paths]
    return subprocess.call(cmd)


def cmd_finish(args: argparse.Namespace) -> int:
    root = git_root(args.repo)
    path = session_path(root)
    session = read_json(path, {})
    if not session.get("active"):
        print("No active guardian session. Run session_guard.py start before an implementation task.")
        return 2
    changed = changed_since_start(root, session)
    project_changed = [p for p in changed if not is_harness_only(p)]
    print(f"Session changed paths: {', '.join(changed) if changed else 'none'}")

    guard_rc = run_guardrail(root, project_changed, args.allow_protected, args.reason)
    if guard_rc:
        print("Completion rejected: a changed project path violates protected-area rules.")
        return guard_rc

    if project_changed and not args.no_memory_required:
        if not has_session_memory(root, str(session.get("session_id", ""))):
            print("Completion rejected: project files changed but no memory record was written for this session.")
            print("Record the validated fix/correction/decision with memory_store.py add, then rerun finish.")
            return 3

    session["active"] = False
    session["finished_at"] = now_iso()
    session["changed_paths"] = changed
    write_json(path, session)
    print("Guardian completion gate passed.")
    return 0


def cmd_status(args: argparse.Namespace) -> int:
    root = git_root(args.repo)
    print(json.dumps(read_json(session_path(root), {}), indent=2, sort_keys=True))
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--repo")
    sub = p.add_subparsers(dest="command", required=True)
    s = sub.add_parser("start")
    s.add_argument("--force", action="store_true")
    s.set_defaults(func=cmd_start)
    s = sub.add_parser("finish")
    s.add_argument("--allow-protected", action="store_true")
    s.add_argument("--reason")
    s.add_argument("--no-memory-required", action="store_true", help="Use only for explicit harness/admin tasks, never to bypass normal engineering memory.")
    s.set_defaults(func=cmd_finish)
    s = sub.add_parser("status")
    s.set_defaults(func=cmd_status)
    return p


def main() -> int:
    args = build_parser().parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
