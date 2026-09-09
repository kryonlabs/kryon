"""Check real-input animation captures without blessing a visual golden."""

import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageStat

CAPTURE_STAGES = (
    "", "-hover-start", "-hover-middle", "-hover-end", "-press-start",
    "-press-end", "-release-end", "-exit-start", "-exit-end",
    "-focus-exit-start", "-focus-exit-end", "-focus-start", "-focus-middle", "-focus-end",
)


def differs(first, second):
    return ImageChops.difference(first, second).getbbox() is not None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--captures", type=Path, required=True)
    args = parser.parse_args()
    # Excludes animated loading indicators and includes only the driven button.
    fixed_states = (12, 130, 748, 400)
    for theme in ("light", "dark", "light-wide", "dark-wide"):
        wide = theme.endswith("-wide")
        control = (16, 892, 752, 944) if wide else (12, 734, 102, 800)
        frames = {}
        for stage in CAPTURE_STAGES:
            with Image.open(args.captures / f"{theme}{stage}.png") as image:
                if image.size != (768, 1024):
                    raise AssertionError("motion capture dimensions changed")
                if image.convert("RGBA").getchannel("A").getextrema() != (255, 255):
                    raise AssertionError(f"{theme}{stage}: opaque capture contains transparency")
                frames[stage] = image.convert("RGB")
        for first, second in [("", "-hover-end"), ("-hover-start", "-hover-middle"),
                              ("-hover-middle", "-hover-end"),
                              ("-press-start", "-press-end"),
                              ("-press-end", "-release-end"),
                              ("-exit-start", "-exit-end"),
                              ("-focus-exit-start", "-focus-exit-end"),
                              ("-focus-start", "-focus-middle"),
                              ("-focus-middle", "-focus-end")]:
            if not differs(frames[first].crop(control), frames[second].crop(control)):
                raise AssertionError(f"{theme}: no rendered transition from {first} to {second}")
        baseline = frames[""].crop(fixed_states)
        # Completed transitions must recover the same material, not merely
        # stop producing visible changes. Include the rim and external field.
        for first, second in [("", "-focus-exit-end"), ("-exit-end", "-focus-end")]:
            if differs(frames[first].crop(control), frames[second].crop(control)):
                raise AssertionError(f"{theme}: settled endpoint differs between {first!r} and {second!r}")
        loading = (78, 410, 748, 450)
        if not differs(frames[""].crop(loading), frames["-hover-middle"].crop(loading)):
            raise AssertionError(f"{theme}: loading indicators did not animate")
        # The dark filled primary has a distinct focused material. Sample
        # inside the face, away from its label and focus rings, so a ring-only
        # animation cannot hide a missing color transition.
        if theme.startswith("dark"):
            face = (100, 910, 110, 929) if wide else (30, 758, 38, 778)
            for first, second in [("-focus-start", "-focus-middle"),
                                  ("-focus-middle", "-focus-end")]:
                if not differs(frames[first].crop(face), frames[second].crop(face)):
                    raise AssertionError(f"dark: focused material did not fade from {first} to {second}")
        if theme == "dark-wide":
            # Sample the lower field outside the face, away from its label.
            # A moving bevel alone must not satisfy the emission transition.
            field = (160, 938, 640, 943)
            blue = {
                stage: ImageStat.Stat(frames[stage].crop(field)).mean[2]
                for stage in ("-hover-end", "-press-end", "-release-end")
            }
            if not (blue["-press-end"] < blue["-hover-end"] and
                    blue["-press-end"] < blue["-release-end"]):
                raise AssertionError(f"dark-wide: pressure did not dim the lower light: {blue}")
        for stage in CAPTURE_STAGES[1:]:
            if differs(baseline, frames[stage].crop(fixed_states)):
                raise AssertionError(f"{theme}: input changed unrelated explicit state rows at {stage}")
        print(f"{theme}: hover, press, release, exit and focus fade; explicit state rows stay fixed")


if __name__ == "__main__":
    main()
