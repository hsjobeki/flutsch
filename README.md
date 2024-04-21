# Flutsch

🚧🚧🚧 Under construction 🚧🚧🚧

> This tool is not production ready yet.

A recursive nix value introspection tool.

**Flutsch** is a dynamic and efficient tool designed to traverse any generic Nix expression (attribute sets) easily.

It introspects nested attributes and functions, presenting dynamic value information in a JSON format (position, value type, etc.). This output can be used for further static analysis tasks, including but not limited to inspection of documentation comments.

Try it out:

```bash
nix run github:hsjobeki/flutsch <path/to/file.nix>
```

This will create a file `values.json` in your current directory.

Flutsch was designed as generic expression treewalker with the boobytraps and footguns of nixpkgs in mind.

since it leverages the native C++ Nix evaluator. It catches all kinds of evaluation errors; and more importantly, it can provide introspection without adding more builtins / changing the nix language itself.

## Demo

![Demo](demos/simple.gif)

## Example

```nix
rec {
  a = {
    # The id function
    inherit a;
  };
}
```

Which essentially contains three entries:

1. The root attrset
  `rec {...}`
1. The binding for `a = ...`
2. The recursive binding `inherit a`

Theoretically there would also be a value `a.a.a` and so on. But flutsch terminates here since it has already introspected all the values.

The result looks then like this:

```json
[
    {
        "binding": {
            "is_root": false,
            "name": "a",
            "pos": {
                "column": 3,
                "file": "test.nix",
                "line": 2
            }
        },
        "value": {
            "children": [
                {
                    "is_root": false,
                    "name": "a",
                    "pos": {
                        "column": 7,
                        "file": "test.nix",
                        "line": 2
                    }
                }
            ],
            "error": false,
            "error_description": null,
            "path": [
                "<root>",
                "a"
            ],
            "pos": {
                "column": 7,
                "file": "test.nix",
                "line": 2
            },
            "type": "attrset"
        }
    },
    {
        "binding": {
            "is_root": true,
            "name": "<root>",
            "pos": {
                "column": 5,
                "file": "test.nix",
                "line": 1
            }
        },
        "value": {
            "children": [
                {
                    "is_root": false,
                    "name": "a",
                    "pos": {
                        "column": 5,
                        "file": "test.nix",
                        "line": 1
                    }
                }
            ],
            "error": false,
            "error_description": null,
            "path": [
                "<root>"
            ],
            "pos": {
                "column": 5,
                "file": "test.nix",
                "line": 1
            },
            "type": "attrset"
        }
    }
]
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
