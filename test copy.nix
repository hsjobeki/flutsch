rec {
  #   __intern = {
  #     a =1;
  #   };
  #   public = { b = 1;};
  # }

  # foo = "This is an error";
  # a = {
  #   inherit lib;
  # };

  # lib = {
  #   map = _f: 1;
  # };
}

# inherit (pkgs.lib) mirrorFunctionArgs; #) makeScopeWithSplicing';

# setFunctionArgs = f: args:
#   { 
#     __functor = self: a: b: c: 1;
#     # __functionArgs = args;
#   };  



# functionArgs = f:
#   if f ? __functor
#   then f.__functionArgs or (functionArgs (f.__functor f))
#   else builtins.functionArgs f;

# mirrorFunctionArgs' =
#   f:
#   let
#     fArgs = functionArgs f;
#   in
#   g:
#   setFunctionArgs g fArgs;

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

# Simple case:

# f = {
#   __functor = self: {a,b}: {x,y}: true;
# };

# Simple primops
# d = map 1;

# Functions that return operate on functions:

# toFunctor
# g = f: h: {
#   __functor = self: f;
# };
# simple (infinte) recursion
# c = { a }: _: c;

# cross (infinite) recursion 
# a Any->b Any-> a ....
# a = _: b;
# b = _: a;


# setFunctionArgs
# setFunctionArgs = f: args:
#   { 
#     __functor = self: f;
#     __functionArgs = args;
#   };

# mirrorFunctionArgs = f:



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
# }
