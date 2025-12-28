local UI = require("kryon.dsl")
local Reactive = require("kryon.reactive")

local count = Reactive.state(0)

local root = UI.Column {
  backgroundColor = "#1a1a1a",
  
  UI.Text {
    text = "Counter Example"
  },
  
  UI.Button {
    text = "Increment",
    onClick = function()
      count:set(count:get() + 1)
    end
  }
}

return { root = root }
