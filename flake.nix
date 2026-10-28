{
  description = "My C/C++ development environment with ncurses";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells.default = pkgs.mkShell {
          # 构建工具和依赖
          packages = with pkgs; [
            gcc
            gnumake
            pkg-config
          ];

          # 库的“开发”输出，提供头文件和 .pc 文件
          buildInputs = with pkgs; [
            ncurses.dev
          ];

          # 进入环境时自动执行的脚本
          shellHook = ''
            echo "C/C++ dev shell loaded. GCC version: $(gcc --version | head -n1)"
          '';
        };
      });
}
