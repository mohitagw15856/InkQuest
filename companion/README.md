# InkQuest companion

The InkQuest companion is a small Python command line tool that compiles Twine
and Ink stories into the on device `.iqs` format and validates their structure.
It has no third party dependencies; the standard library is enough.

## Install

```sh
cd companion
pip install -e .            # optional; or just run as a module
```

Either invocation works:

```sh
inkquest --help                 # if installed
python3 -m inkquest --help      # without installing
```

## Compile

```sh
# Twine (Twee 3 or published HTML, Harlowe or SugarCube)
inkquest compile story.twee -o story.iqs

# Ink (compile first with inklecate, then feed the JSON)
inklecate -j -o story.json story.ink
inkquest compile story.json --from ink -o story.iqs
```

The format is inferred from the extension (`.twee`, `.tw`, `.html` are Twine;
`.json` is Ink) and can be forced with `--from`. Compilation runs validation
first and refuses to write a story with dead ends or unreachable passages unless
`--force` is given. Useful options: `--title`, `--uid`, `-o/--output`.

## Validate

```sh
inkquest validate story.twee     # from source
inkquest validate story.iqs      # from a compiled file
```

Reports the start passage, the passage and ending counts, and any unreachable or
dead end passages. Exit status is non zero when problems are found.

## Supported story features

See [../docs/FORMAT.md](../docs/FORMAT.md) for the exact supported subsets of
Twine and Ink. In short: links, variables, assignment, conditional choices and
conditional text, and integer or boolean expressions. Unsupported constructs
raise a clear error naming the passage and the construct rather than producing a
silently wrong story.

## Tests

```sh
python3 -m pytest
```

The suite covers the expression compiler, the `.iqs` writer and a reference
reader/runtime, both front ends, the validator and the CLI.
