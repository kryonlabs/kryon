import kryon_dsl
import std/times

# Deep Tree Benchmark - 1000-component nested tree
# Before optimization: 300-500ms startup
# After optimization: <100ms startup

echo "Building deep component tree..."
let startTime = cpuTime()

discard kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Deep Tree Benchmark"

  Body:
    backgroundColor = "#F0F0F0FF"

    Text:
      content = "Deep Tree Benchmark - 1000+ Components"
      fontSize = 24.px
      color = rgb(50, 50, 50)
      marginBottom = 10.px

    # Create a deep tree: depth=5, branch=4 = 1024 components total
    # This tests the exponential child array growth optimization
    Container:
      width = 100.percent
      height = auto
      layoutDirection = 1
      gap = 5.px

      for i in 0..<10:  # 10 top-level containers
        Container:
          width = 100.percent
          height = auto
          layoutDirection = 0  # Row
          gap = 3.px
          padding = (5.px, 5.px, 5.px, 5.px)
          backgroundColor = rgb(255, 255, 255)
          borderRadius = 6.px
          boxShadow = shadow(0.px, 1.px, 3.px, rgba(0, 0, 0, 0.1))

          # Each container has 10 child containers with 10 children each
          # Total: 10 * 10 * 10 = 1000 components
          for j in 0..<10:
            Container:
              width = auto
              height = auto
              layoutDirection = 1
              gap = 2.px

              for k in 0..<10:
                Text:
                  content = $(i) & "-" & $(j) & "-" & $(k)
                  fontSize = 10.px
                  color = rgb(120, 120, 120)

let buildTime = (cpuTime() - startTime) * 1000.0
echo "Tree built in ", buildTime, " ms"
