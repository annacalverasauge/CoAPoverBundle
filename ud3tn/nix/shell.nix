# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

{ pkgs, ... }:

pkgs.mkShell rec {

  hardeningDisable = [ "all" ];

  # Derive build inputs from custom packages
  inputsFrom = with pkgs; [
    pyd3tn
    python-ud3tn-utils
    ud3tn
  ];

  # Prepare a virtual python enviroment for development
  venvDir = "./.venv";
  buildInputs = with pkgs.python3Packages; [ python venvShellHook ];
  postVenvCreation = ''
    unset SOURCE_DATE_EPOCH
    pip install -e ./python-ud3tn-utils
    pip install -e ./pyd3tn
  '';
  postShellHook = "unset SOURCE_DATE_EPOCH";

  packages = with pkgs; [
    clang-tools
    cppcheck
    llvmPackages.libcxxClang
    nixpkgs-fmt
    protobuf
    python3Packages.flake8
    python3Packages.pytest
    python3Packages.pytest-asyncio
    sqlite
  ] ++ lib.optional (stdenv.isLinux) gdb;
}
