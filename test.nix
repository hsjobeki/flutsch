# let r = { inherit r; };
# in r
rec {
  a = 1;
  # a = 1;
  # b = a;
  # a = 1;
  # b = a;
  # a = {
  #   # The id function
  #   id = x: x;
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
  errors = { ${throw "Some funny boobytrap" } = 1;  }; 


  # # import invalid file
  # c = import ./b.nix; 

  # lib = import <nixpkgs/lib>;
  # writers = (import <nixpkgs> {}).writers;
}