# Web host status

Kryon currently has no supported web host. Ziran's maintained code generators
target C, C++, and Go; they do not generate JavaScript or browser DOM code.
Kryon's supported hosts are listed in [BACKENDS.md](BACKENDS.md).

The former `.kry` to JavaScript compiler and handwritten JavaScript widget
runtime were removed from the current repositories. The old experiment is
available in Git history, but it is not an implementation or compatibility
path. Historical plans and test ledgers may still describe that experiment;
they are not current web support documentation.

Any future browser support must let a Ziran application import Kryon as an
ordinary library. Language code generation and browser capability bindings
belong in Ziran. Kryon's web host, if built, should be written in Ziran and
should reuse the same widget, tree, layout, and input decisions as the other
hosts. It needs its own executable package tests before being listed as a
supported backend.
