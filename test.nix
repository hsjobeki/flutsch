let 
  pkgs = import <nixpkgs> {};
in
rec {
  # inherit (pkgs.stdenv) mkDerivation;
  inherit (pkgs.lib) makeScopeWithSplicing';

  # fn = 
  #   x: 
  #   {pkgs ,lib ? pkgs.lib, ...}:
  #   { 
  #     __functor = super: {arg ? null}: 
  #       { __functor = self: {arg ? null}: 
  #         {x,y}: 
  #           1; 
  #       };
  #   };

  # User facing arguments:
  # x, op, list
  # f = map;
  # g = map (x: x);
  # fix = f: let x = f x; in x;
  # f = {
  #   __functor = s: {a,b}: {x,y}: true;
  # };
  # inherit (pkgs) lib;

  # fn = args@{b,c ? {},d, ...}: x: c: d: 1;
  # a = {
  #   # The id function
  #   inherit a;
  # };
  # a = {
  #   # The id function
  # e = throw "This is a boobytrap";
  # };

  # b = {
  #   inherit a;
  #   inherit b;
  # };

  # b = {
  #   inherit a;
  #   inherit b;
  # };

  # errors = { a = throw "Some funny boobytrap"; };
  # errors = throw "Some funny boobytrap"; 
  # errors = { ${throw "Some funny boobytrap" } = 1;  }; 


  # # import invalid file
  # c = import ./b.nix; 

  # lib = import <nixpkgs/lib>;
  # writers = (import <nixpkgs> {}).writers;
}