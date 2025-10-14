## Custom Font Text Example
##
## Demonstrates how to configure Kryon to use bundled fonts using the new Resources DSL.
## This shows the declarative approach to font management within the application structure.

import std/os
import ../src/kryon

let sourceDir = splitPath(currentSourcePath()).head
for candidate in [sourceDir, sourceDir / "assets"]:
  if candidate.len > 0 and dirExists(candidate):
    addFontSearchDir(candidate)

let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 420
    windowTitle = "Custom Font Example - DSL Resources"

  Resources:
    Font:
      name = "Fira Sans"
      sources = [
        "assets/FiraSans-Regular.ttf",
        "assets/FiraSans-Regular.woff2"
      ]

  Body:
    backgroundColor = "#0A0F1E"
    fontFamily = "Fira Sans"  # Set a default font for the entire body

    Column:
      gap = 24
      padding = 48

      Text:
        # This text inherits fontFamily from Body
        text = "Fira Sans (local asset)"
        fontSize = 36
        color = "#F9FAFB"

      Text:
        text = "We define fonts in the Resources section."
        fontSize = 24
        color = "#CBD5F5"

      Text:
        text = "Fonts are now part of the application structure."
        fontSize = 18
        color = "#94A3B8"
