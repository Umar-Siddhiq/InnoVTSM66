#!/usr/bin/env python3
import argparse
import json
import os
import subprocess


def run_git(args, cwd):
    p = subprocess.run(["git", *args], cwd=cwd, text=True, capture_output=True)
    return {"ok": p.returncode == 0, "stdout": p.stdout.strip(), "stderr": p.stderr.strip()}


def main():
    parser = argparse.ArgumentParser(description="Collect concise Git evidence for engineering documentation.")
    parser.add_argument("--repo", default=".", help="Repository path")
    parser.add_argument("--diff-limit", type=int, default=12000, help="Maximum diff characters")
    args = parser.parse_args()

    repo = os.path.abspath(args.repo)
    result = {
        "repo": repo,
        "branch": run_git(["rev-parse", "--abbrev-ref", "HEAD"], repo),
        "head": run_git(["rev-parse", "HEAD"], repo),
        "status": run_git(["status", "--short"], repo),
        "last_commit": run_git(["log", "-1", "--pretty=format:%H%n%ad%n%an%n%s", "--date=iso"], repo),
        "diff_stat": run_git(["diff", "--stat"], repo),
        "diff": run_git(["diff", "--no-ext-diff", "--unified=3"], repo),
    }

    diff = result["diff"]["stdout"]
    if len(diff) > args.diff_limit:
        result["diff"]["stdout"] = diff[:args.diff_limit] + "\n...TRUNCATED..."
        result["diff"]["truncated"] = True
    else:
        result["diff"]["truncated"] = False

    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
