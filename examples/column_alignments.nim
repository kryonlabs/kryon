## Column Alignments Demo
##
## Demonstrates different mainAxisAlignment values for Column layouts
## Uses staticFor to generate multiple alignment examples from data

import ../src/kryon

# Define alignment configurations using named tuples
const alignments = [
  (
    name: "start (flex-start)",
    value: "start",
    gap: 15,
    colors: ["#ef4444", "#dc2626", "#b91c1c"]
  ),
  (
    name: "center",
    value: "center",
    gap: 15,
    colors: ["#06b6d4", "#0891b2", "#0e7490"]
  ),
  (
    name: "end (flex-end)",
    value: "end",
    gap: 15,
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
    windowWidth = 1000
    windowHeight = 800
    windowTitle = "Column Alignments Demo"

  Body:
    backgroundColor = "#f8fafc"
    color = "#1e293b"

    Row:
      mainAxisAlignment = "start"
      crossAxisAlignment = "stretch"
      gap = 20
      padding = 0

      # Use staticFor to generate a column for each alignment

      static:
        for alignment in alignments:
          Container:
            width = 150
            height = 740
            backgroundColor = "#ffffff"
            padding = 15

            Column:
              mainAxisAlignment = "start"
              crossAxisAlignment = "center"
              gap = 10

              Text:
                text = "mainAxisAlignment: " & alignment.name
                fontSize = 12
                fontWeight = "bold"

              Container:
                width = 120
                height = 600
                backgroundColor = "#f1f5f9"
                padding = 8

                Column:
                  mainAxisAlignment = alignment.value
                  crossAxisAlignment = "center"
                  gap = alignment.gap

                  Container:
                    width = 80
                    height = 60
                    backgroundColor = alignment.colors[0]

                  Container:
                    width = 80
                    height = 80
                    backgroundColor = alignment.colors[1]

                  Container:
                    width = 80
                    height = 70
                    backgroundColor = alignment.colors[2]

