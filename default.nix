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

  patchPhase = ''
    # ensure the mkfs and fsck scripts, which are just wrappers around
    # 'bcachefs', are patched to refer to the right location inside the
    # nix store. (you wouldn't expect built tools to call random outside
    # utilities, in general, but the exact tools they were built with.)
    #
    # TODO FIXME: it would be better to fix this in the 'install' target,
    # however, so this works with any bog-standard installation

    substituteInPlace fsck.bcachefs --replace bcachefs $out/bin/bcachefs
    substituteInPlace mkfs.bcachefs --replace bcachefs $out/bin/bcachefs
  '';

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
