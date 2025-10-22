# NixOS overlay for Kryon
# This allows users to add Kryon to their system configuration

final: prev:

{
  kryon = prev.callPackage ./default.nix { };

  # Optional: Create aliases for convenience
  kryon-static = prev.callPackage ./default.nix { static = true; };
  kryon-dynamic = prev.callPackage ./default.nix { static = false; };
}