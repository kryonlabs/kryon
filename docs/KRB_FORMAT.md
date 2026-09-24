# Ziran portable bundles

Kryon no longer defines or loads `.krb` cartridges. Ziran owns the portable
`.zib` format; Kryon modules are linked into a bundle only when a program
imports them. Programs without Kryon use the same format and loader.

See [Ziran's `.zib` documentation](https://github.com/kryonlabs/ziran/blob/master/docs/ZIB.md)
for the current bundle contract. Existing `.krb` files must be recompiled from
source; their bytes are not `.zib` bundles.

The removed `.krb` specification remains available in Git history.
