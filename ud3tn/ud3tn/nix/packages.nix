# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

{ pkgs, ... }:

let
  version = "0.14.1";
in

rec {
  ud3tn = pkgs.stdenv.mkDerivation {
    pname = "ud3tn";
    inherit version;

    src = pkgs.lib.sourceByRegex ../. [
      "Makefile"
      "^components.*"
      "^external.*"
      "^generated.*"
      "^include.*"
      "^mk.*"
    ];

    buildInputs = [
      pkgs.sqlite
    ];

    buildPhase = ''
      make type=release optimize=yes -j $NIX_BUILD_CORES ud3tn
    '';

    installPhase = ''
      mkdir -p $out/bin
      cp build/posix/ud3tn $out/bin/
    '';
  };

  pyd3tn = with pkgs.python3Packages; buildPythonPackage {
    pname = "pyd3tn";
    inherit version;
    src = ../pyd3tn;
    format = "pyproject";
    nativeBuildInputs = [ setuptools ];
    propagatedBuildInputs = [ cbor2 ];
  };

  python-ud3tn-utils = with pkgs.python3Packages; buildPythonPackage {
    pname = "python-ud3tn-utils";
    inherit version;
    src = ../python-ud3tn-utils;
    format = "pyproject";
    nativeBuildInputs = [ setuptools ];
    propagatedBuildInputs = [ cbor2 protobuf pyd3tn ];
  };
}
