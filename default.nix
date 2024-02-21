{ stdenv
, lib
, srcDir ? null
, boost
, clang-tools
, cmake
, difftastic
, makeWrapper
, meson
, ninja
, nix
, nlohmann_json
, catch2_3
, pkg-config
}:

let
  filterMesonBuild = builtins.filterSource
    (path: type: type != "directory" || baseNameOf path != "build");
in
stdenv.mkDerivation {
  pname = "flutsch";
  version = "1.0.0";
  src = if srcDir == null then filterMesonBuild ./. else srcDir;
  checkInputs = [
    catch2_3
  ];
  doCheck = true;
  buildInputs = [
    nlohmann_json
    nix
    boost
  ];
  nativeBuildInputs = [
    makeWrapper
    meson
    pkg-config
    ninja
    # nlohmann_json can be only discovered via cmake files
    cmake
  ] ++ (lib.optional stdenv.cc.isClang [ clang-tools ]);

  postInstall = ''
    wrapProgram "$out/bin/flutsch" --prefix PATH : ${difftastic}/bin
  '';

  meta = {
    # description = "Nix unit test runner";
    # homepage = "https://github.com/adisbladis/nix-unit";
    license = lib.licenses.gpl3;
    # maintainers = with lib.maintainers; [ adisbladis ];
    platforms = lib.platforms.unix;
    mainProgram = "flutsch";
  };
}
