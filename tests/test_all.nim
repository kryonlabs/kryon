import unittest
import options

import ../src/kryon/dsl
import ../src/kryon/core

suite "Case statement DSL":
  test "creates conditional elements per branch":
    var currentRoute = "Home"

    let root = Container:
      case currentRoute:
      of "Home":
        Text:
          text = "Home"
      of "Settings":
        Text:
          text = "Settings"
      else:
        Text:
          text = "Fallback"

    var conditionals: seq[Element] = @[]
    for child in root.children:
      if child.kind == ekConditional:
        conditionals.add(child)

    check conditionals.len == 3

    check conditionals[0].condition() == true
    check conditionals[1].condition() == false
    check conditionals[2].condition() == false

    currentRoute = "Settings"
    check conditionals[0].condition() == false
    check conditionals[1].condition() == true
    check conditionals[2].condition() == false

    currentRoute = "Unknown"
    check conditionals[0].condition() == false
    check conditionals[1].condition() == false
    check conditionals[2].condition() == true

    let homeText = conditionals[0].trueBranch.children[0].getProp("text")
    check homeText.isSome
    check homeText.get.getString() == "Home"
