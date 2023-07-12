{
  description = "A basic flake to help develop qwlbox";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nix-filter.url = "github:numtide/nix-filter";
    qwlroots = {
      url = "github:vioken/qwlroots";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
      inputs.nix-filter.follows = "nix-filter";
    };
  };

  outputs = { self, flake-utils, nix-filter, nixpkgs, qwlroots }@input:
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" "riscv64-linux" ]
      (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          qwlbox = pkgs.qt6Packages.callPackage ./nix {
            qwlroots = qwlroots.packages.${system}.qwlroots-qt6-wlroots-git;
            nix-filter = nix-filter.lib;
          };
        in
        rec {
          packages.default = qwlbox;

          devShell = pkgs.mkShell { 
            packages = with pkgs; [
              wayland-utils
              foot
              weston
            ];

            inputsFrom = [
              packages.default
            ];

            shellHook = ''
              echo "welcome to qwlbox"
              echo "wlroots: $(pkg-config --modversion wlroots)"
              echo "wayland-server: $(pkg-config --modversion wayland-server)"
              #export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false"
              export MESA_DEBUG=1
              export EGL_LOG_LEVEL=debug
              export LIBGL_DEBUG=verbose
              export WAYLAND_DEBUG=1
            '';
          };

          apps.${system}.default = {
            type = "app";
            program = packages.default;
          };
        }
      );
}
