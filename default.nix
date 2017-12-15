{ nixpkgs ? (import ./nix/nixpkgs.nix)
}:

with nixpkgs;

stdenv.mkDerivation rec {
  name = "bcachefs-tools-${version}";
  version = "git";

  src = lib.cleanSource ./.; # NOTE: ignore .git, otherwise things get weird!

  nativeBuildInputs = [ git pkgconfig ];
  buildInputs =
    [ liburcu libuuid libaio zlib attr keyutils
      libsodium libscrypt
    ];

  enableParallelBuilding = true;
  makeFlags =
    [ "PREFIX=$(out)"
    ];

  meta = with stdenv.lib; {
    description = "Userspace tools for bcachefs";
    homepage    = http://bcachefs.org;
    license     = licenses.gpl2;
    platforms   = platforms.linux;
    maintainers =
      [ "Kent Overstreet <kent.overstreet@gmail.com>"
      ];
  };
}
