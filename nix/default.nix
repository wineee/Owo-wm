{ stdenv
, lib
, nix-filter
, fetchFromGitHub
, cmake
, pkg-config
, qtbase
, wrapQtAppsHook
, wlroots
, wayland
, wayland-protocols
, wayland-scanner
, pixman
, mesa
, vulkan-loader
, libinput
, xorg
, seatd
, qwlroots
}:

stdenv.mkDerivation rec {
  pname = "qwlbox";
  version = "0.0.1";

  src = nix-filter.filter {
    root = ./..;

    exclude = [
      ".git"
      "LICENSE"
      "README.md"
      (nix-filter.matchExt "nix")
    ];
  };

  nativeBuildInputs = [
    cmake
    pkg-config
    wrapQtAppsHook
    wayland-scanner
  ];

  buildInputs = [
    qtbase
    qwlroots
    wlroots
    wayland
    wayland-protocols
    pixman
    mesa
    vulkan-loader
    libinput
    xorg.libXdmcp
    xorg.xcbutilerrors
    seatd
  ];

  meta = with lib; {
    description = "a better tinywm";
    homepage = "https://github.com/wineee/qwlbox";
    license = licenses.gpl3Plus;
    platforms = platforms.linux;
    maintainers = with maintainers; [ rewine ];
  };
}

