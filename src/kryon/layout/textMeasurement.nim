## Text Measurement Interface
##
## This module defines the text measurement interface that backends must implement
## to provide text measurement capabilities to the layout engine.
##
## The layout engine needs to know text dimensions to calculate proper element
## sizes and positions, but the actual measurement is backend-specific.
##
## Backends must implement a type with a `measureText` proc that takes:
##   - text: string
##   - fontSize: int
##   - Returns: tuple[width: float, height: float]
##
## The layout engine uses duck typing, so any type with this interface will work.
