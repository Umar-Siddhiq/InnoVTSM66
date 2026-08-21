#!/usr/bin/env python3
"""Generate a compact, evidence-only project map and a detailed JSON index."""
from __future__ import annotations

import argparse
import collections
import datetime as dt
import hashlib
import json
import os
import subprocess
from pathlib import Path
from typing import Iterable

EXCLUDE_PARTS = {
    ".git", ".ai", ".agents", ".claude", "node_modules", "dist", "build", ".next",
    ".venv", "venv", "__pycache__", "bin", "obj", "target", "vendor", "coverage",
}
SECRET_NAMES = {".env", ".env.local", ".env.production", ".env.development", "credentials.json", "secrets.json"}
LANGS = {
    ".c": "C", ".h": "C/C++ Header", ".cc": "C++", ".cpp": "C++", ".cxx": "C++",
    ".hpp": "C++ Header", ".py": "Python", ".js": "JavaScript", ".jsx": "JavaScript/JSX",
    ".ts": "TypeScript", ".tsx": "TypeScript/TSX", ".java": "Java", ".kt": "Kotlin",
    ".go": "Go", ".rs": "Rust", ".swift": "Swift", ".dart": "Dart", ".cs": "C#",
    ".php": "PHP", ".rb": "Ruby", ".sql": "SQL", ".sh": "Shell", ".ps1": "PowerShell",
    ".html": "HTML", ".css": "CSS", ".scss": "SCSS", ".vue": "Vue", ".svelte": "Svelte",
}
MANIFESTS = {
    "package.json", "pyproject.toml", "requirements.txt", "poetry.lock", "Pipfile", "Cargo.toml",
    "go.mod", "pom.xml", "build.gradle", "build.gradle.kts", "CMakeLists.txt", "Makefile",
    "platformio.ini", "idf_component.yml", "pubspec.yaml", "composer.json", "Gemfile",
}
ENTRY_NAMES = {
    "main.c", "main.cpp", "main.cc", "main.py", "app.py", "server.py", "index.js", "index.ts",
    "App.tsx", "App.jsx", "manage.py", "Program.cs", "main.go", "lib.rs", "main.rs",
}


def now_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()


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


def safe_path(rel: str) -> bool:
    p = Path(rel)
    if any(part in EXCLUDE_PARTS for part in p.parts):
        return False
    name = p.name
    if name in SECRET_NAMES:
        return False
    if name.startswith(".env") and not any(x in name.lower() for x in ("example", "sample", "template")):
        return False
    return True


def git_files(root: Path) -> list[str]:
    try:
        out = subprocess.check_output(
            ["git", "-C", str(root), "ls-files", "-co", "--exclude-standard"],
            stderr=subprocess.DEVNULL,
            text=True,
        )
        values = sorted({x.strip().replace("\\", "/") for x in out.splitlines() if x.strip()})
        return [x for x in values if safe_path(x)]
    except (subprocess.CalledProcessError, FileNotFoundError):
        values: list[str] = []
        for base, dirs, files in os.walk(root):
            dirs[:] = [d for d in dirs if d not in EXCLUDE_PARTS]
            for name in files:
                rel = (Path(base) / name).relative_to(root).as_posix()
                if safe_path(rel):
                    values.append(rel)
        return sorted(values)


def git_text(root: Path, args: list[str]) -> str:
    try:
        return subprocess.check_output(["git", "-C", str(root), *args], stderr=subprocess.DEVNULL, text=True).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def status_paths(root: Path) -> list[str]:
    out = git_text(root, ["status", "--porcelain=v1"])
    result: list[str] = []
    for line in out.splitlines():
        if len(line) < 4:
            continue
        value = line[3:]
        if " -> " in value:
            value = value.split(" -> ", 1)[1]
        value = value.strip('"').replace("\\", "/")
        if safe_path(value):
            result.append(value)
    return sorted(set(result))


def high_churn(root: Path, limit_commits: int = 200, top: int = 20) -> list[dict[str, int | str]]:
    out = git_text(root, ["log", f"-n{limit_commits}", "--name-only", "--pretty=format:"])
    counts = collections.Counter(
        line.strip().replace("\\", "/")
        for line in out.splitlines()
        if line.strip() and safe_path(line.strip())
    )
    return [{"path": path, "changes": count} for path, count in counts.most_common(top)]


def file_info(root: Path, rel: str) -> dict[str, object]:
    path = root / rel
    try:
        stat = path.stat()
        size = stat.st_size
    except OSError:
        size = 0
    return {
        "path": rel,
        "extension": path.suffix.lower(),
        "language": LANGS.get(path.suffix.lower()),
        "size": size,
    }


def top_dirs(files: Iterable[str]) -> list[dict[str, object]]:
    counts = collections.Counter()
    for rel in files:
        parts = Path(rel).parts
        counts[parts[0] if len(parts) > 1 else "."] += 1
    return [{"path": p, "files": n} for p, n in counts.most_common(20)]


def generate(root: Path) -> dict[str, object]:
    files = git_files(root)
    infos = [file_info(root, rel) for rel in files]
    langs = collections.Counter(i["language"] for i in infos if i["language"])
    manifests = [x for x in files if Path(x).name in MANIFESTS]
    entries = [x for x in files if Path(x).name in ENTRY_NAMES]
    ci = [x for x in files if x.startswith(".github/workflows/") or x in {".gitlab-ci.yml", "Jenkinsfile", "azure-pipelines.yml"}]
    tests = [x for x in files if any(part.lower() in {"test", "tests", "spec", "specs"} for part in Path(x).parts) or Path(x).name.lower().startswith(("test_", "spec_"))]
    large_source = sorted(
        [i for i in infos if i["language"]], key=lambda x: int(x["size"]), reverse=True
    )[:20]
    branch = git_text(root, ["branch", "--show-current"])
    head = git_text(root, ["rev-parse", "--short", "HEAD"])
    remote = git_text(root, ["config", "--get", "remote.origin.url"])
    return {
        "generated_at": now_iso(),
        "root_name": root.name,
        "git": {"branch": branch, "head": head, "remote": remote},
        "counts": {"files": len(files), "tests": len(tests)},
        "languages": [{"name": name, "files": count} for name, count in langs.most_common()],
        "top_directories": top_dirs(files),
        "manifests": manifests,
        "entrypoint_candidates": entries[:40],
        "ci": ci,
        "test_candidates": tests[:80],
        "dirty_paths": status_paths(root),
        "high_churn": high_churn(root),
        "largest_source_files": large_source,
        "files": infos,
    }


def render_md(data: dict[str, object]) -> str:
    git = data["git"]
    assert isinstance(git, dict)
    lines = [
        "# Project Map",
        "",
        f"Generated: `{data['generated_at']}`",
        f"Repository: `{data['root_name']}`",
        f"Git: branch `{git.get('branch') or 'unknown'}`, HEAD `{git.get('head') or 'unknown'}`",
        "",
        "> Evidence-only inventory. Candidate entrypoints are filename-based; verify behavior in source before editing.",
        "",
        "## Scale",
        "",
        f"- Files indexed: **{data['counts']['files']}**",
        f"- Test-like files: **{data['counts']['tests']}**",
        "",
        "## Languages",
        "",
    ]
    for item in data["languages"][:12]:
        lines.append(f"- {item['name']}: {item['files']} files")
    if not data["languages"]:
        lines.append("- No recognized source extensions found.")

    def section(title: str, values: list[str], cap: int = 20) -> None:
        lines.extend(["", f"## {title}", ""])
        if values:
            lines.extend(f"- `{x}`" for x in values[:cap])
        else:
            lines.append("- None detected.")

    section("Build and dependency manifests", data["manifests"])
    section("Entrypoint candidates", data["entrypoint_candidates"])
    section("CI and automation", data["ci"])
    section("Current dirty paths", data["dirty_paths"])

    lines.extend(["", "## Top-level areas", ""])
    for item in data["top_directories"][:15]:
        lines.append(f"- `{item['path']}`: {item['files']} files")

    lines.extend(["", "## High-churn files", ""])
    if data["high_churn"]:
        for item in data["high_churn"][:15]:
            lines.append(f"- `{item['path']}`: {item['changes']} commits in sampled history")
    else:
        lines.append("- No Git history available.")

    lines.extend(["", "## Largest source files", ""])
    for item in data["largest_source_files"][:15]:
        lines.append(f"- `{item['path']}`: {item['size']} bytes ({item['language']})")
    if not data["largest_source_files"]:
        lines.append("- No recognized source files found.")

    lines.extend([
        "",
        "## Retrieval rule",
        "",
        "Use this map to choose a small set of relevant files. Search symbols and regression memory before reading broad directories. Do not infer architecture from filenames alone.",
    ])
    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--repo")
    args = p.parse_args()
    root = git_root(args.repo)
    data = generate(root)
    out_dir = root / ".ai"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "PROJECT_MAP.json").write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (out_dir / "PROJECT_MAP.md").write_text(render_md(data), encoding="utf-8")
    print(f"Wrote {out_dir / 'PROJECT_MAP.md'}")
    print(f"Wrote {out_dir / 'PROJECT_MAP.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
