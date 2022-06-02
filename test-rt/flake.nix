{
  outputs = { self, nixpkgs }:
    with import nixpkgs { system = "x86_64-linux"; };
    let
      vars = runCommand ""
        {
          buildInputs = [ libsForQt5.qt5.qtbase ];
          nativeBuildInputs = [ libsForQt5.qt5.wrapQtAppsHook ];
          dontWrapQtApps = true;
        } ''
        mkdir -p $out
        echo echo \$QT_PLUGIN_PATH \> $out/QT_PLUGIN_PATH >> a.sh
        echo echo \$XDG_DATA_DIRS \> $out/XDG_DATA_DIRS >> a.sh
        echo echo \$XDG_CONFIG_DIRS \> $out/XDG_CONFIG_DIRS >> a.sh
        chmod 777 a.sh
        wrapQtApp ./a.sh
        sh a.sh
      '';

      cling_wrapped = runCommand "" { nativeBuildInputs = [ makeWrapper ]; } ''
        mkdir -p $out/bin
        makeWrapper ${cling}/bin/cling $out/bin/cling \
          --add-flags "-isystem ${rtaudio}/include" \
          --add-flags "-L ${rtaudio}/lib"
      '';
    in
    {
      devShells.x86_64-linux.default = mkShell {
        buildInputs = [
          cmake
          cmake-format
          libsForQt5.qt5.qtbase
          libsForQt5.qt5.qtcharts
          rtaudio
          treefmt
          clang
          clang-tools
          cling_wrapped
          gdb
          valgrind
        ];

        shellHook = ''
          export QT_PLUGIN_PATH=$(cat ${vars}/QT_PLUGIN_PATH)
          export XDG_DATA_DIRS=$(cat ${vars}/XDG_DATA_DIRS)
          export XDG_CONFIG_DIRS=$(cat ${vars}/XDG_CONFIG_DIRS)
        '';
      };
    };
}
