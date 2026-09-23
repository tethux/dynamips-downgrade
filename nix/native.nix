{
  stdenv,
  xmake,
  clang,
  git,
  pkg-config,
  libelf,
  libpcap,
  libnsl,
  src,
}:
stdenv.mkDerivation {
  pname = "dynamips-downgrade-native";
  version = "0.2.25";
  inherit src;

  nativeBuildInputs = [
    xmake
    clang
    git
    pkg-config
  ];
  buildInputs = [
    libelf
    libpcap
    libnsl
  ];

  configurePhase = ''
    runHook preConfigure
    export HOME="$TMPDIR"
    xmake g --network=private
    xmake f -m release --system_packages=y --enable_gen_eth=y \
      --cxxflags="-isystem${stdenv.cc.cc}/include/c++/${stdenv.cc.cc.version} -isystem${stdenv.cc.cc}/include/c++/${stdenv.cc.cc.version}/${stdenv.hostPlatform.config} -isystem${stdenv.cc.libc.dev}/include" -y
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    xmake build -y dynamips-bindings
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    xmake install -o "$out" dynamips-bindings dynamips-core
    runHook postInstall
  '';
}
