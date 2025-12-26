## Plugin Git Operations
##
## Handles cloning and pulling plugins from git repositories

import std/[os, osproc, strformat, strutils]

# =============================================================================
# Git Operations
# =============================================================================

proc gitClone*(url: string, targetDir: string, depth: int = 0): bool =
  ## Clone a git repository
  ## If depth > 0, performs a shallow clone with --depth=N
  ##
  ## Returns true on success, false on failure

  # Ensure parent directory exists
  let parentDir = parentDir(targetDir)
  if not dirExists(parentDir):
    try:
      createDir(parentDir)
    except:
      echo &"✗ Failed to create directory: {parentDir}"
      return false

  # Check if target already exists
  if dirExists(targetDir):
    echo &"⚠ Directory already exists: {targetDir}"
    echo "  Remove it first or use a different path"
    return false

  # Normalize git URL (add https:// if needed)
  var normalizedUrl = url
  if not url.startsWith("http://") and not url.startsWith("https://") and not url.startsWith("git@"):
    # Assume github-style URL: github.com/user/repo
    normalizedUrl = "https://" & url

  # Build git clone command
  var cmd = "git clone"
  if depth > 0:
    cmd.add(&" --depth={depth}")
  cmd.add(&" \"{normalizedUrl}\" \"{targetDir}\"")

  # Execute clone
  let (output, exitCode) = execCmdEx(cmd)

  if exitCode != 0:
    echo &"✗ Git clone failed:"
    echo output
    return false

  return true

proc gitPull*(repoDir: string): bool =
  ## Pull latest changes from git remote
  ##
  ## Returns true on success, false on failure

  if not dirExists(repoDir):
    echo &"✗ Directory not found: {repoDir}"
    return false

  if not dirExists(repoDir / ".git"):
    echo &"✗ Not a git repository: {repoDir}"
    return false

  # Execute git pull
  let (output, exitCode) = execCmdEx(&"git -C \"{repoDir}\" pull")

  if exitCode != 0:
    echo &"✗ Git pull failed:"
    echo output
    return false

  # Show what was pulled
  if "Already up to date" in output:
    echo "  (Already up to date)"
  else:
    echo output

  return true

proc gitGetCurrentCommit*(repoDir: string): string =
  ## Get the current commit hash
  let (output, exitCode) = execCmdEx(&"git -C \"{repoDir}\" rev-parse HEAD")

  if exitCode == 0:
    return output.strip()
  else:
    return ""

proc gitGetRemoteUrl*(repoDir: string): string =
  ## Get the remote origin URL
  let (output, exitCode) = execCmdEx(&"git -C \"{repoDir}\" remote get-url origin")

  if exitCode == 0:
    return output.strip()
  else:
    return ""

proc isGitRepository*(path: string): bool =
  ## Check if a path is a git repository
  dirExists(path / ".git")
