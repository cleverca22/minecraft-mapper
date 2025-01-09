{ stdenv, zlib, libpng, qpdf, nlohmann_json, nodejs }:

stdenv.mkDerivation {
  name = "biome-thingy";
  buildInputs = [ zlib libpng nlohmann_json ];
  nativeBuildInputs = [ qpdf nodejs ];
}
