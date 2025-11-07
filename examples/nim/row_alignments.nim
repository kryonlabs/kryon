## Row Alignments Demo
##
## Demonstrates different mainAxisAlignment values for Row layouts
## Uses static for to generate multiple alignment examples from data

import kryon_dsl

# Define alignment configurations using named tuples
const alignments = [
  (
    name: "start (flex-start)",
    value: "start",
    gap: 20,
    colors: ["#ef4444", "#dc2626", "#b91c1c"]
  ),
  (
    name: "center",
    value: "center",
    gap: 20,
    colors: ["#06b6d4", "#0891b2", "#0e7490"]
  ),
  (
    name: "end (flex-end)",
    value: "end",
    gap: 20,
    colors: ["#8b5cf6", "#7c3aed", "#6d28d9"]
  ),
  (
    name: "spaceEvenly",
    value: "spaceEvenly",
    gap: 0,
    colors: ["#64748b", "#475569", "#334155"]
  ),
  (
    name: "spaceAround",
    value: "spaceAround",
    gap: 0,
    colors: ["#f59e0b", "#d97706", "#b45309"]
  ),
  (
    name: "spaceBetween",
    value: "spaceBetween",
    gap: 0,
    colors: ["#10b981", "#059669", "#047857"]
  )
]

# Define the UI
let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 740
    windowTitle = "Row Alignments Demo"

  Body:
    backgroundColor = "#f8fafc"
    color = "#1e293b"

    Column:
      mainAxisAlignment = "start"
      crossAxisAlignment = "stretch"
      gap = 20
      padding = 20

      # Use static for to generate a row for each alignment
      static:
        for alignment in alignments:
          Container:
            width = 760
            height = 100
            backgroundColor = "#ffffff"
            padding = 10

            Column:
              mainAxisAlignment = "start"
              crossAxisAlignment = "start"
              gap = 8

              Text:
                text = "mainAxisAlignment: " & alignment.name
                fontSize = 12
                fontWeight = "bold"

              Container:
                width = 740
                height = 60
                backgroundColor = "#f1f5f9"
                padding = 8

                Row:
                  mainAxisAlignment = alignment.value
                  crossAxisAlignment = "center"
                  gap = alignment.gap

                  Container:
                    width = 80
                    height = 40
                    backgroundColor = alignment.colors[0]

                  Container:
                    width = 100
                    height = 40
                    backgroundColor = alignment.colors[1]

                  Container:
                    width = 90
                    height = 40
                    backgroundColor = alignment.colors[2]

