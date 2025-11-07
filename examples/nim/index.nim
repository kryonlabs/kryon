## Examples Index
##
## Minimal navigation across all demos discovered in the examples directory.

import std/sequtils
import std/strutils
import std/os

import kryon_dsl

let exampleFiles = toSeq(walkDir("examples", relative = true))
  .filterIt(it.kind == pcFile and it.path.endsWith(".nim") and it.path != "index.nim")
  .mapIt(it.path)
  .sorted()

proc renderCard(fileName: string): Element =
  Container:
    width = 360
    padding = 20
    backgroundColor = "#1F2937"
    borderColor = "#374151"
    borderWidth = 1

    Column:
      gap = 12

      Text:
        text = fileName
        fontSize = 20
        color = "#F9FAFB"

      Link:
        text = "Open"
        to = fileName

let app* = kryonApp:
  Header:
    windowWidth = 1280
    windowHeight = 800
    windowTitle = "Kryon Examples"

  Body:
    backgroundColor = "#0F172A"

    Column:
      gap = 24
      padding = 32

      Text:
        text = "Examples"
        fontSize = 36
        color = "#F9FAFB"


      for fileValue in exampleFiles:
        renderCard(fileValue)

