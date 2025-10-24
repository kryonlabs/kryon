## Kryon UI Framework
##
## Declarative UI framework for multiple platforms
##
## Example:
## ```nim
## import kryon
##
## let app = kryonApp:
##   Container:
##     width: 400
##     height: 300
##
##     Text:
##       text: "Hello Kryon!"
##       color: "#FFD700"
## ```

import kryon/core
import kryon/dsl
import kryon/fonts
import kryon/components/canvas

# Re-export public API
export core
export dsl
export fonts
export canvas

# Version information
const
  kryonVersion* = "0.1.0"
  kryonAuthor* = "Kryon Labs"
