#!/usr/bin/env python3
"""Write a P4 soundcraft manifest from files that already exist.

This does not render audio. Host renders stay local because GitHub-hosted
runners do not have REAPER. Missing inputs are a failure, not an empty pass.
"""
import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_sha() -> str:
    try:
        return subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--vst3", type=Path, required=True)
    parser.add_argument("--wav", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--sample-rate", type=int, default=48000)
    parser.add_argument("--block-size", type=int, default=0)
    args = parser.parse_args()
    if not args.vst3.exists():
        print(f"missing vst3: {args.vst3}", flush=True)
        return 2
    manifest = {
        "run_id": datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
        "git_sha": git_sha(),
        "vst3": str(args.vst3),
        "vst3_sha256": sha256(args.vst3),
        "sample_rate": args.sample_rate,
        "block_size": args.block_size,
        "wav": None,
        "wav_sha256": None,
    }
    if args.wav is not None:
        if not args.wav.exists():
            print(f"missing wav: {args.wav}", flush=True)
            return 2
        manifest["wav"] = str(args.wav)
        manifest["wav_sha256"] = sha256(args.wav)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"wrote {args.out}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
