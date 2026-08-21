#!/usr/bin/env python3
"""Representative end-to-end self-test for Project Guardian scripts."""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent


def run(args: list[str], cwd: Path, expect: int = 0) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(args, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if proc.returncode != expect:
        print(proc.stdout)
        raise AssertionError(f"Expected exit {expect}, got {proc.returncode}: {' '.join(args)}")
    return proc


def py(name: str, *args: str) -> list[str]:
    return [sys.executable, str(HERE / name), *args]


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="project-guardian-test-") as td:
        repo = Path(td)
        run(["git", "init"], repo)
        run(["git", "config", "user.email", "guardian@example.invalid"], repo)
        run(["git", "config", "user.name", "Project Guardian Test"], repo)
        (repo / "src").mkdir()
        (repo / "src" / "gps.c").write_text("int gps_enabled = 1;\n", encoding="utf-8")
        (repo / "src" / "app.c").write_text("int main(void) { return 0; }\n", encoding="utf-8")
        (repo / "README.md").write_text("# Demo\n", encoding="utf-8")
        run(["git", "add", "."], repo)
        run(["git", "commit", "-m", "baseline"], repo)

        run(py("bootstrap_project.py", "--repo", str(repo), "--apply"), repo)
        assert (repo / ".ai" / "PROJECT_MAP.md").exists()
        assert (repo / ".ai" / "PROJECT_MAP.json").exists()
        assert "PROJECT-GUARDIAN:START" in (repo / "AGENTS.md").read_text(encoding="utf-8")

        run(py("guardrail.py", "--repo", str(repo), "check-paths", "src/gps.c"), repo, expect=2)
        run(py("guardrail.py", "--repo", str(repo), "--allow-protected", "--reason", "explicit test override", "check-paths", "src/gps.c"), repo)

        run(["git", "add", "."], repo)
        run(["git", "commit", "-m", "guardian bootstrap"], repo)

        run(py("session_guard.py", "--repo", str(repo), "start"), repo)
        (repo / "src" / "app.c").write_text("int main(void) { return 1; }\n", encoding="utf-8")
        run(py("session_guard.py", "--repo", str(repo), "finish"), repo, expect=3)
        run(py(
            "memory_store.py", "--repo", str(repo), "add",
            "--kind", "fix", "--title", "App return behavior",
            "--summary", "Changed app return value for self-test.",
            "--files", "src/app.c", "--validation", "self-test fixture",
            "--guard", "Keep GPS code unchanged."
        ), repo)
        run(py("session_guard.py", "--repo", str(repo), "finish"), repo)

        run(["git", "add", "."], repo)
        run(["git", "commit", "-m", "app change"], repo)
        run(py("session_guard.py", "--repo", str(repo), "start"), repo)
        (repo / "src" / "gps.c").write_text("int gps_enabled = 0;\n", encoding="utf-8")
        run(py(
            "memory_store.py", "--repo", str(repo), "add",
            "--kind", "correction", "--title", "GPS must stay enabled",
            "--summary", "Self-test records the attempted protected change.",
            "--critical", "--files", "src/gps.c",
            "--guard", "Do not disable GPS while diagnosing satellite count."
        ), repo)
        run(py("session_guard.py", "--repo", str(repo), "finish"), repo, expect=2)
        run(py("session_guard.py", "--repo", str(repo), "finish", "--allow-protected", "--reason", "explicit self-test authorization"), repo)

        search = run(py("memory_store.py", "--repo", str(repo), "search", "GPS satellite"), repo)
        assert "GPS must stay enabled" in search.stdout
        protected = json.loads((repo / ".ai" / "PROTECTED_AREAS.json").read_text(encoding="utf-8"))
        assert any(x.get("pattern") == "**/*gps*" for x in protected.get("areas", []))
        index_text = (repo / ".ai" / "MEMORY_INDEX.md").read_text(encoding="utf-8")
        assert "GPS must stay enabled" in index_text
        print("Project Guardian self-test PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
