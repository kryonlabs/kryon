"""Link a native profile's verified package artifacts before system libraries."""
import argparse
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys

from renderer_artifact import renderer_link, renderer_metadata


MARKER = '--kryon-artifact-libraries'


def link(args):
    selections = args.artifacts.split(' ')
    if not selections or not all(re.fullmatch(r'[A-Za-z0-9_-]+:[A-Za-z0-9_-]+', value)
                                 for value in selections):
        raise ValueError('invalid native artifact selections')
    command = args.command
    if command[:1] == ['--']:
        command = command[1:]
    if not command or command.count(MARKER) != 1:
        raise ValueError('native artifact link needs a compiler and one library marker')
    helper = args.toolchain / 'scripts/ziran_artifacts.py'
    if not helper.is_file():
        raise ValueError('native artifacts need a newer Ziran toolchain; update ziran')
    spec = importlib.util.spec_from_file_location('ziran_artifacts', helper)
    artifacts = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(artifacts)
    env = dict(os.environ, CC=command[0])
    flags = []
    for selection in selections:
        artifact = artifacts.request(selection, args.platform, project=args.project,
                                     ziran=args.toolchain / 'build/bin/ziran', env=env)
        if 'renderer.json' in artifact.rows:
            flags.extend(renderer_link(renderer_metadata(artifact)))
        else:
            flags.extend(artifact.link_flags(env=env))
    index = command.index(MARKER)
    return subprocess.run([*command[:index], *flags, *command[index + 1:]],
                          cwd=args.project, env=env).returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--platform', required=True)
    parser.add_argument('--artifacts', required=True)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        return link(args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print('kryon native artifact link: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
