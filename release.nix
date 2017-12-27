{ system  ? builtins.currentSystem
, nixpkgs ? (import ./nix/nixpkgs.nix { inherit system; })
}:

{
  bcachefs-tools = nixpkgs.pkgs.callPackage ./default.nix { };
}
