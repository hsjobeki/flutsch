# Flutsch

🚧🚧🚧 Under construction 🚧🚧🚧

> This tool is not production ready yet.

A recursive nix value introspection tool.

**Flutsch** is a dynamic and efficient tool designed to traverse any generic Nix expression (attribute sets) easily.

It introspects nested attributes and functions, presenting dynamic value information in a JSON format (position, value type, etc.). This output can be used for further static analysis tasks, including but not limited to inspection of documentation comments.

It was designed as generic expression treewalker with the boobytraps and footguns of nixpkgs in mind.

since it leverages the native C++ Nix evaluator. It catches all kinds of evaluation errors; and more importantly, it can provide introspection without adding more builtins / changing the nix language itself.

## Example

```nix
rec {
  a = {
    # The id function
    id = x: x;
  };
  b = {
    inherit a;
    inherit b;
  };
}
```

The result is the following hashmap:

```bash
Key: (<root> @ 0x7ffff5254040), Value:  - (attrset @ /home/johannes/git/flutsch/test.nix:1:5) 
    children: 
        - b : (b @ 0x7ffff5254060)
        - a : (a @ 0x7ffff5254080)

Key: (a @ 0x7ffff5254080), Value: <root>.a - (attrset @ /home/johannes/git/flutsch/test.nix:2:7) 
    children: 
        - id : (id @ 0x7ffff5254020)

Key: (id @ 0x7ffff5254020), Value: <root>.a.id - (lambda @ /home/johannes/git/flutsch/test.nix:4:10)
Key: (b @ 0x7ffff5254060), Value: <root>.b - (attrset @ /home/johannes/git/flutsch/test.nix:6:7) 
    children: 
        - b : (b @ 0x7ffff5254060)
        - a : (a @ 0x7ffff5254080)
```

Which can be used (e.g. via it json representation) for further static analysis, visualization tasks, or generating documentation.

## Key Features

- **Deep Traversal:** Effortlessly explores any Nix attribute set without much configuration.
- **Error Handling:** With its native error handling in the C++ Nix evaluator, Flutsch cannot fail.
- **Introspection Output:** The native c++ bindings allow to retrieve information that is internal to the evaluation.
- **Static Analysis Readiness:** Flutsch's output serves as a prime candidate for static analysis, especially for scrutinizing documentation comments and understanding the structure and relations within Nix expressions.

## Usage

`flutsch ./default.nix`

**Flutsch** can be granularly configured via a json config file:

Simply pass `--config <file_path.json>` to the invocation.

## Contributing

TODO
