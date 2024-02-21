rec {
  a = {
    # The id function
    id = x: x;
  };
  b = {
    inherit a;
    inherit b;
  };

  # b = {
  #   inherit a;
  #   inherit b;
  # };

  # errors = { a = throw "Some funny boobytrap"; };

  # # import invalid file
  # c = import ./b.nix; 

  # lib = import <nixpkgs/lib>;
}