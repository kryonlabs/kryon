#!/usr/bin/env python3
import argparse
import json
import os
import urllib.request
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT.parent / "showcase" / "projects"
DEFAULT_OUTPUT = ROOT / "docs" / "site" / "showcase-data.json"
LOCAL_BANNERS = {
    "krait": "showcase/krait.png",
    "pass": "showcase/pass.png",
    "ktrem": "showcase/ktrem.png"
}


def load_projects(source):
    projects = []
    for path in sorted(source.glob("*.json")):
        with path.open("r", encoding="utf-8") as fh:
            projects.append(json.load(fh))
    return projects


def github_stars(owner, repo):
    url = f"https://api.github.com/repos/{owner}/{repo}"
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "kryon-showcase-updater"
    }
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request, timeout=15) as response:
        data = json.load(response)
    return int(data.get("stargazers_count", 0))


def ranked_entry(project, offline):
    repo = project.get("repository") or {}
    author = project.get("author") or {}
    stars = None
    unranked = bool(project.get("unranked"))
    if (not offline and not unranked and repo.get("platform") == "github" and
            not repo.get("private") and repo.get("owner") and repo.get("name")):
        try:
            stars = github_stars(repo["owner"], repo["name"])
        except Exception:
            stars = None
    elif not unranked and "stars" in project:
        stars = int(project["stars"])
    else:
        unranked = True
    repo_url = repo.get("url")
    homepage = project.get("homepage") or repo_url or "#"
    author_name = author.get("name") or repo.get("owner") or project["name"]
    author_url = author.get("url") or repo_url or homepage

    entry = {
        "rank": None,
        "slug": project["slug"],
        "name": project["name"],
        "summary": project["summary"],
        "author": {
            "name": author_name,
            "url": author_url
        },
        "homepage": homepage,
        "banner": LOCAL_BANNERS.get(project["slug"], project["banner"]["url"]),
        "stars": stars,
        "tags": project["tags"],
        "featured": bool(project.get("featured")),
        "unranked": unranked
    }
    if repo_url and not repo.get("private"):
        entry["repository"] = repo_url
    return entry


def main():
    parser = argparse.ArgumentParser(description="Update Kryon static showcase data from project metadata.")
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="Directory containing projects/*.json files.")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="Output showcase-data.json path.")
    parser.add_argument("--offline", action="store_true", help="Do not refresh stars from remote APIs.")
    args = parser.parse_args()

    entries = [ranked_entry(project, args.offline) for project in load_projects(args.source)]
    ranked = [entry for entry in entries if not entry.get("unranked")]
    unranked = [entry for entry in entries if entry.get("unranked")]

    ranked.sort(key=lambda entry: (-(entry["stars"] or 0), entry["name"].lower()))
    for index, entry in enumerate(ranked, 1):
        entry["rank"] = index

    output = {
        "schemaVersion": 1,
        "updatedAt": date.today().isoformat(),
        "source": "https://github.com/kryonlabs/showcase",
        "starPlatforms": ["github"],
        "projects": ranked[:10] + sorted(unranked, key=lambda entry: entry["name"].lower())
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as fh:
        json.dump(output, fh, indent=2)
        fh.write("\n")


if __name__ == "__main__":
    main()
