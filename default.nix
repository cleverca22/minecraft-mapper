{ stdenv, zlib, libpng, qpdf, nlohmann_json, nodejs, valgrind }:

stdenv.mkDerivation {
  name = "biome-thingy";
  src = ./.;
  buildInputs = [ zlib libpng nlohmann_json ];
  nativeBuildInputs = [ qpdf nodejs valgrind ];
}
