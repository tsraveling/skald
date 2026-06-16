# LSP SIGNPOST

The **Skald LSP** is a long-lived process: JSON-RPC over stdin/stdout (framed
by `Content-Length`). It parses each open `.ska`, and hands back diagnostics,
hover, go-to-definition, references, completion, and outline.

NOTE: **stdout is the protocol channel.** Any stray print corrupts it — so
`main.cpp` silences all skald library logging.

## Plumbing

- **main.cpp** — entry loop. Read message → `Server::handle_message` → repeat.
  Kills library debug logging (`dbg_out_on=false`, log_level OFF).
- **transport.{h,cpp}** — read/write `Content-Length`-framed JSON; make
  response/notification/error envelopes.
- **lsp_types.h** — LSP wire structs (Position, Range, Diagnostic, Completion,
  DocumentSymbol, …) + JSON (de)serialization. Positions here are **0-based**.
- **server.{h,cpp}** — request router + all editor state. Holds open
  `Document`s, the `Workspace`, the `CodexCache`, the `ProjectIndex`. Handles
  didOpen/didChange (re-parse + publish diagnostics), definition (same-file
  first, then cross-file via project index), references, hover, completion,
  documentSymbol, watched-file changes (invalidate codex, re-parse everything).

## Workspace / project discovery

- **workspace.{h,cpp}** — workspace root, recursive `.ska` listing, URI↔path.
- **codex_cache.{h,cpp}** — finds the "mother codex" for a `.ska` (nearest
  ancestor dir with exactly one `.codex`; reuses the engine's
  `FileManager::find_project_root`), parses it once, caches the `Skald::Codex`
  (methods + globals) by path. Codex parse errors surface against the codex's
  own URI. Invalidated when the `.codex` changes.
- **project_index.{h,cpp}** — whole-project model. Parses every `.ska` under the
  codex, records each module's `@let` vars + outgoing `GO` edges, builds the GO
  graph. A module imports (as **thread variables**) the `@let` vars of any
  module that can reach it via GO. Powers cross-file go-to-definition / hover:
  resolve a variable to a codex global or an upstream module's `@let` line.

## Parsing (reuses the library grammar)

The library already turns `.ska` text into a `Skald::Module` and collects
structured `ParseError`s (syntax, method existence/arg checks, declaration
errors). We piggyback on that and add LSP-only bookkeeping.

- **lsp_parse_state.h** — `LspParseState : Skald::ParseState`. Adds a
  `SymbolOccurrence` list (name, kind, def-vs-ref, 0-based range) recorded
  during the parse — the index behind hover/definition/references/completion.
  Also zeroes a couple of uninitialized base-class declaration flags.
- **lsp_actions.h** — PEGTL action overrides that populate the symbol list as
  rules fire: block-tag definitions (full dotted `parent.child` path), move
  targets (records the **absolute tag the library already resolved** from
  relative/dotted forms), `@let` var defs, mutations, method calls (flags
  whether a call is rvalue/conditional vs inline), GO file refs, testbed sets.
- **document.{h,cpp}** — one open file. Runs the parse, turns
  `ParseState::errors` straight into diagnostics (severity passed through), then
  runs **LSP-only semantic checks** the library doesn't own:
  - transition target `->` doesn't resolve to a block → ERROR
  - `GO file.ska` whose file doesn't exist (relative to codex) → ERROR
  - `@let` var shadows a codex global → WARNING
  - unused module var → WARNING
  - action-typed method used as a value/conditional → ERROR
  - testbed sets a name that isn't a module/global/thread var → ERROR

  Also the symbol-lookup queries (`symbol_at`, `find_definition`,
  `find_references`, `open_transitions`).

## Analysis features

- **move_resolver.{h,cpp}** — the single LSP-side place that reasons about
  block tags. Library resolves relative/dotted move targets to absolute tags at
  parse time; this builds the block-tag index, answers "does this target
  exist?", and computes **open transitions** (targets referenced by `->` with
  no defining block yet) for `# `/`## `/`### ` completion.
- **analyzer.{h,cpp}** — stateless feature builders over a `Document`:
  completion (context-sniffs the line: `->` → block tags, `:` → codex methods,
  `{`/`(?`/`~` → vars, `GO ` → files, `# ` → open transitions), document
  outline, and hover (block beat counts + `---` doc comments; variable scope
  global/module/local; method signature from codex).
