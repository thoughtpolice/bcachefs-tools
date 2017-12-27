{ stdenv
, git, pkgconfig
, libscrypt, libsodium, liburcu, libuuid, libaio, zlib, attr, keyutils
}:

stdenv.mkDerivation rec {
  name = "bcachefs-tools-${version}";
  version = "git";

  # NOTE: ignore .git, otherwise things get weird!
  src = stdenv.lib.cleanSource ./.;

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
