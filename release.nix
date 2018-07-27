{ bcachefs-tools ? builtins.fetchGit ./.
, system ? builtins.currentSystem
, nixpkgs ? (import ./nix/nixpkgs.nix { inherit system; })
}:

let
  version = "0.1pre${builtins.toString bcachefs-tools.revCount}_${bcachefs-tools.shortRev}";
in
{
  bcachefs-tools = nixpkgs.pkgs.callPackage ./default.nix { inherit version; };
}
