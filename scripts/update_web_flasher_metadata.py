#!/usr/bin/env python3
"""
Generate metadata (version, commit hash, file sizes, SHA256) for the CrossPoint Web Flasher
and update ESP Web Tools manifests prior to GitHub Pages deployment.
"""

import os
import sys
import json
import hashlib
import datetime
import subprocess

def get_git_info():
    commit = os.environ.get("GITHUB_SHA", "")
    if commit:
        commit = commit[:7]
    else:
        try:
            commit = subprocess.check_output(
                ["git", "rev-parse", "--short=7", "HEAD"],
                text=True
            ).strip()
        except Exception:
            commit = "unknown"

    branch = os.environ.get("GITHUB_REF_NAME", "")
    if not branch:
        try:
            branch = subprocess.check_output(
                ["git", "rev-parse", "--abbrev-ref", "HEAD"],
                text=True
            ).strip()
        except Exception:
            branch = "custom-crosspoint"

    return commit, branch

def sha256_file(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def update_metadata(repo_root):
    web_dir = os.path.join(repo_root, "web-flasher")
    firmware_dir = os.path.join(web_dir, "firmware")
    manifests_dir = os.path.join(web_dir, "manifests")

    commit, branch = get_git_info()
    built_at = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

    # Index firmware files
    files_info = {}
    if os.path.isdir(firmware_dir):
        for fname in sorted(os.listdir(firmware_dir)):
            if fname.endswith(".bin"):
                fpath = os.path.join(firmware_dir, fname)
                size = os.path.getsize(fpath)
                sha = sha256_file(fpath)
                files_info[fname] = {
                    "size": size,
                    "sha256": sha
                }

    version_data = {
        "commit": commit,
        "branch": branch,
        "builtAt": built_at,
        "files": files_info
    }

    # Write version.json
    version_file = os.path.join(web_dir, "version.json")
    with open(version_file, "w") as f:
        json.dump(version_data, f, indent=2)
    print(f"Updated {version_file} with commit {commit} ({branch})")

    # Update manifest versions
    version_label = f"{branch} ({commit})"
    manifest_files = [os.path.join(web_dir, "manifest.json")]
    if os.path.isdir(manifests_dir):
        for mname in os.listdir(manifests_dir):
            if mname.endswith(".json"):
                manifest_files.append(os.path.join(manifests_dir, mname))

    for mpath in manifest_files:
        if os.path.isfile(mpath):
            try:
                with open(mpath, "r") as f:
                    mdata = json.load(f)
                mdata["version"] = version_label
                with open(mpath, "w") as f:
                    json.dump(mdata, f, indent=2)
                print(f"Updated {mpath} version -> '{version_label}'")
            except Exception as e:
                print(f"Warning: could not update {mpath}: {e}")

if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    update_metadata(root)
