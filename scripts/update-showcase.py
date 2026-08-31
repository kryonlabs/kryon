#!/usr/bin/env python3
import argparse
import base64
import datetime as dt
import hashlib
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


USER_AGENT = "kryon-showcase-builder"


def request(url):
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": USER_AGENT,
    }
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    return urllib.request.Request(url, headers=headers)


def fetch_json(url):
    with urllib.request.urlopen(request(url), timeout=20) as response:
        return json.load(response)


def fetch_bytes(url):
    with urllib.request.urlopen(request(url), timeout=30) as response:
        return response.read()


def fetch_banner_bytes(url):
    parsed = urllib.parse.urlparse(url)
    parts = parsed.path.lstrip("/").split("/")
    if parsed.netloc == "raw.githubusercontent.com" and len(parts) >= 4:
        owner, repo, ref = parts[0], parts[1], parts[2]
        path = "/".join(parts[3:])
        api_url = (
            f"https://api.github.com/repos/{owner}/{repo}/contents/"
            f"{urllib.parse.quote(path)}?ref={urllib.parse.quote(ref)}"
        )
        data = fetch_json(api_url)
        if data.get("encoding") == "base64" and isinstance(data.get("content"), str):
            return base64.b64decode(data["content"])
    return fetch_bytes(url)


def load_local_projects(projects_dir):
    paths = sorted(projects_dir.glob("*.json"))
    paths += sorted(projects_dir.glob("*/project.json"))
    projects = []
    for path in paths:
        project = json.loads(path.read_text(encoding="utf-8"))
        if path.name == "project.json":
            project["_project_dir"] = str(path.parent)
        projects.append(project)
    return projects


def load_github_projects(repo, ref):
    owner, name = repo.split("/", 1)
    listing_url = (
        f"https://api.github.com/repos/{owner}/{name}/contents/projects"
        f"?ref={urllib.parse.quote(ref)}"
    )
    entries = fetch_json(listing_url)
    projects = []
    for entry in sorted(entries, key=lambda item: item["name"]):
        if entry.get("type") == "file" and entry["name"].endswith(".json"):
            data = fetch_bytes(entry["download_url"])
        elif entry.get("type") == "dir":
            project_url = (
                f"https://api.github.com/repos/{owner}/{name}/contents/projects/"
                f"{urllib.parse.quote(entry['name'])}/project.json"
                f"?ref={urllib.parse.quote(ref)}"
            )
            project_entry = fetch_json(project_url)
            data = fetch_bytes(project_entry["download_url"])
        else:
            continue
        projects.append(json.loads(data.decode("utf-8")))
    return projects


def github_stars(repository):
    if not repository:
        return None
    if repository.get("platform") != "github":
        return None
    if repository.get("private") or not repository.get("owner") or not repository.get("name"):
        return None
    owner = repository["owner"]
    name = repository["name"]
    try:
        repo = fetch_json(f"https://api.github.com/repos/{owner}/{name}")
    except urllib.error.HTTPError as exc:
        if exc.code in (401, 403, 404):
            return None
        raise
    stars = repo.get("stargazers_count")
    return stars if isinstance(stars, int) else None


def banner_name(project):
    url_path = urllib.parse.urlparse(project["banner"]["url"]).path
    suffix = Path(url_path).suffix.lower()
    if suffix not in (".png", ".jpg", ".jpeg", ".webp"):
        suffix = ".png"
    return f"{project['slug']}{suffix}"


def download_banner(project, banner_dir):
    banner_dir.mkdir(parents=True, exist_ok=True)
    name = banner_name(project)
    project_dir = project.get("_project_dir")
    if project_dir:
        local_path = Path(project_dir) / Path(urllib.parse.urlparse(project["banner"]["url"]).path).name
        if local_path.is_file():
            data = local_path.read_bytes()
        else:
            data = fetch_banner_bytes(project["banner"]["url"])
    else:
        data = fetch_banner_bytes(project["banner"]["url"])
    stem = Path(name).stem
    suffix = Path(name).suffix
    digest = hashlib.sha256(data).hexdigest()[:12]
    name = f"{stem}-{digest}{suffix}"
    path = banner_dir / name
    path.write_bytes(data)
    return f"showcase/{name}"


def site_project(project, banner_path, stars):
    repository = project.get("repository") or {}
    author = project.get("author") or {}
    repository_url = repository.get("url")
    homepage = project.get("homepage") or repository_url or "#"
    author_name = author.get("name") or repository.get("owner") or project["name"]
    author_url = author.get("url") or repository_url or homepage
    data = {
        "slug": project["slug"],
        "name": project["name"],
        "summary": project["summary"],
        "author": {
            "name": author_name,
            "url": author_url,
        },
        "homepage": homepage,
        "banner": banner_path,
        "bannerAlt": project.get("banner", {}).get("alt", f"{project['name']} app banner"),
        "tags": project["tags"],
        "featured": bool(project.get("featured")),
    }
    if repository_url and not repository.get("private"):
        data["repository"] = repository_url
    if isinstance(stars, int):
        data["stars"] = stars
    return data


def ranked_stars(project):
    return project["stars"] if isinstance(project.get("stars"), int) else -1


def rank_projects(projects, limit):
    ranked = sorted(
        projects,
        key=lambda project: (
            -ranked_stars(project),
            project["name"].lower(),
        ),
    )[:limit]
    rank = 1
    for project in ranked:
        if isinstance(project.get("stars"), int):
            project["rank"] = rank
            rank += 1
    return ranked


def main():
    parser = argparse.ArgumentParser(description="Generate Kryon showcase site data.")
    parser.add_argument("--projects-dir", type=Path)
    parser.add_argument("--source-repo", default="kryonlabs/showcase")
    parser.add_argument("--source-ref", default="master")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--banner-dir", type=Path, required=True)
    parser.add_argument("--limit", type=int, default=10)
    args = parser.parse_args()

    if args.projects_dir:
        projects = load_local_projects(args.projects_dir)
        source = str(args.projects_dir)
    else:
        projects = load_github_projects(args.source_repo, args.source_ref)
        source = f"https://github.com/{args.source_repo}/tree/{args.source_ref}/projects"

    site_projects = []
    for project in projects:
        try:
            stars = github_stars(project.get("repository"))
        except (KeyError, urllib.error.URLError, TimeoutError, json.JSONDecodeError) as exc:
            print(f"showcase: could not count {project.get('slug', '<unknown>')}: {exc}", file=sys.stderr)
            stars = None
        try:
            banner_path = download_banner(project, args.banner_dir)
        except (KeyError, urllib.error.URLError, TimeoutError) as exc:
            raise SystemExit(f"showcase: could not download banner for {project.get('slug', '<unknown>')}: {exc}")
        site_projects.append(site_project(project, banner_path, stars))

    data = {
        "schemaVersion": 1,
        "updatedAt": dt.datetime.now(dt.timezone.utc).date().isoformat(),
        "source": source,
        "starPlatforms": ["github"],
        "projects": rank_projects(site_projects, args.limit),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
