{
  description = "A flake for building and running PlotJuggler";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
          config.qt5.enable = true;
        };

        data-tamer-src = pkgs.fetchzip {
          url = "https://github.com/PickNikRobotics/data_tamer/archive/refs/tags/1.0.3.zip";
          sha256 = "sha256-hGfoU6oK7vh39TRCBTYnlqEsvGLWCsLVRBXh3RDrmnY=";
        };

        wasmer-src =
          let
            version = "v7.0.1";
            base = "https://github.com/wasmerio/wasmer/releases/download/${version}";
            sources = {
              "x86_64-linux" = {
                url = "${base}/wasmer-linux-amd64.tar.gz";
                sha256 = "10a55885b11eb51b06bb24ff184facde8c2a83c252782a0c04e7a46926630d72";
              };
              "aarch64-linux" = {
                url = "${base}/wasmer-linux-aarch64.tar.gz";
                sha256 = "21d6968d33defa4a31d878022d261667a8fa8abbfe96007d5f4f28564b7fa372";
              };
              "x86_64-darwin" = {
                url = "${base}/wasmer-darwin-amd64.tar.gz";
                sha256 = "3a0f44a3aae570b0870d4573fa663c7f0c96a2f9550e38eb22c3be7c77658a1e";
              };
              "aarch64-darwin" = {
                url = "${base}/wasmer-darwin-arm64.tar.gz";
                sha256 = "3eff017389fb838b0b5af607a4d392edc6039e76343984fcd24307aa027d67ee";
              };
            };
            info = sources.${system};
          in
          pkgs.runCommandNoCC "wasmer-src" { } ''
            mkdir -p $out
            tar xzf ${pkgs.fetchurl { inherit (info) url sha256; }} -C $out
          '';

        plotjuggler-pkg = pkgs.qt5.mkDerivation {
          pname = "plotjuggler";
          version = "3.10.11";

          src = ./.;
          patches = [ ./nix/arrow.patch ];

          postPatch = ''
            substituteInPlace cmake/find_or_download_data_tamer.cmake \
              --replace "URL" "SOURCE_DIR" \
              --replace "https://github.com/PickNikRobotics/data_tamer/archive/refs/tags/1.0.3.zip" "${data-tamer-src}"

            sed -i \
              -e 's|URL ''${WASMER_URL}|SOURCE_DIR ${wasmer-src}|' \
              -e '/URL_HASH SHA256=/d' \
              cmake/download_wasmer.cmake

            sed -i 's|zstd::libzstd_static|zstd::libzstd_shared|g' \
              plotjuggler_plugins/DataStreamPlotJugglerBridge/CMakeLists.txt

            sed -i \
              -e 's|zstd::libzstd_static|zstd::libzstd_shared|g' \
              -e 's|LZ4::lz4_static|LZ4::lz4_shared|g' \
              plotjuggler_plugins/DataLoadMCAP/CMakeLists.txt

            rm cmake/find_or_download_fmt.cmake
            rm cmake/find_or_download_fastcdr.cmake
            rm cmake/find_or_download_zstd.cmake

            substituteInPlace CMakeLists.txt \
              --replace "include(cmake/find_or_download_fmt.cmake)" "find_package(fmt REQUIRED)" \
              --replace "find_or_download_fmt()" ""

            substituteInPlace CMakeLists.txt \
              --replace "include(cmake/find_or_download_fastcdr.cmake)" "find_package(fastcdr REQUIRED)" \
              --replace "find_or_download_fastcdr()" ""
            find . -name "CMakeLists.txt" -exec sed -i 's/fastcdr::fastcdr/fastcdr/g' {} +

          '';

          cmakeFlags = [
            "-DPLJ_USE_SYSTEM_LUA=ON"
            "-DPLJ_USE_SYSTEM_NLOHMANN_JSON=ON"
          ];


          nativeBuildInputs = [ pkgs.cmake pkgs.qt5.wrapQtAppsHook pkgs.makeWrapper ];

          buildInputs = with pkgs; [
            libsForQt5.qt5.qtwebsockets
            libsForQt5.qt5.qtx11extras
            qt5.qtsvg
            qt5.qtimageformats
            qt5.qtdeclarative
            libsForQt5.qt5.qtwayland
            libxcb
            libxinerama
            wasmtime
            wasmtime.dev
            zeromq
            sqlite
            lua
            nlohmann_json
            fmt
            fastcdr
            lz4
            zstd
            mosquitto
            protobuf_21
            xorg.libX11
            xorg.libxcb
            xorg.xcbutil
            xorg.xcbutilkeysyms
            arrow-cpp
          ];
          dontWrapQtApps = true;

          postInstall = ''
            wrapProgram $out/bin/plotjuggler \
              --prefix QT_PLUGIN_PATH : "${pkgs.qt5.qtbase}/${pkgs.qt5.qtbase.qtPluginPrefix}" \
              --prefix QT_PLUGIN_PATH : "${pkgs.libsForQt5.qt5.qtwayland.bin}/${pkgs.qt5.qtbase.qtPluginPrefix}" \
              --set-default QT_QPA_PLATFORM xcb
          '';

          meta = with pkgs.lib; {
            description = "A tool to plot streaming data, fast and easy";
            homepage = "https://github.com/PlotJuggler/PlotJuggler";
            license = licenses.mpl20;
            platforms = platforms.linux ++ platforms.darwin;
          };
        };

      in
      {
        packages.default = plotjuggler-pkg;
        packages.plotjuggler = plotjuggler-pkg;

        apps.default = {
          type = "app";
          program = "${plotjuggler-pkg}/bin/plotjuggler";
        };
        apps.plotjuggler = self.apps.${system}.default;

        devShells.default = pkgs.mkShell {
          packages = [
            pkgs.cmake
            pkgs.qt5.full
            pkgs.qt5.qtsvg
            pkgs.qt5.qtimageformats
            pkgs.qt5.qtdeclarative
            pkgs.arrow-cpp
            pkgs.zeromq
            pkgs.sqlite
            pkgs.lua
            pkgs.nlohmann_json
            pkgs.fmt
            pkgs.fastcdr
            pkgs.lz4
            pkgs.zstd
            pkgs.mosquitto
            pkgs.protobuf
            pkgs.codespell
            pkgs.xorg.libX11
            pkgs.xorg.libxcb
            pkgs.xorg.xcbutil
            pkgs.xorg.xcbutilkeysyms
          ];
        };
      }
    );
}
