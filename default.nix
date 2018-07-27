{ version
, stdenv, git, pkgconfig, perl
, zstd, lz4, zlib
, libscrypt, libsodium, liburcu, libuuid, libaio
, attr, keyutils
}:

stdenv.mkDerivation {
  name = "bcachefs-tools-${version}";
  inherit version;

  # NOTE: ignore .git, otherwise things get weird!
  src = stdenv.lib.cleanSource ./.;

  nativeBuildInputs = [ git pkgconfig perl ]; /* perl is for 'pod2man' */
  buildInputs =
    [ liburcu libuuid libaio zlib attr keyutils
      libsodium libscrypt zstd lz4
    ];

  enableParallelBuilding = true;
  makeFlags =
    [ "PREFIX=$(out)"
    ];

  patchPhase = ''
    substituteInPlace ./Makefile \
      --replace '0.1-nogit' '${version}' \
      --replace '/etc/initramfs-tools' "$out/etc/initramfs-tools"
  '';

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
