# Skald LSP Server

A Language Server Protocol implementation for `.ska` files, providing real-time diagnostics, navigation, completions, and hover info. It reuses the existing PEGTL grammar and parse actions from the Skald engine directly -- no grammar duplication.

## Building

From the repo root:

```bash
cmake -B build -DSKALD_BUILD_LSP=ON
cmake --build build --target skald_lsp
```

Builds binary at `build/skald_lsp`. The first build will fetch `nlohmann/json` via CMake FetchContent (cached after that).

If you only want the LSP and don't need skalder or the shared library:

```bash
cmake -B build -DSKALD_BUILD_LSP=ON -DSKALD_BUILD_SKALDER=OFF -DSKALD_BUILD_SHARED=OFF
cmake --build build --target skald_lsp
```

## Neovim Setup

Add this to your Neovim config somewhere. I have it in my `after/plugins/lsp.lua` file but you can put it wherever you like.

Note that this location is my local LSP build artifact, generated above; post-alpha we'll have a better way of doing this!

```lua
-- Skald LSP
vim.lsp.config("skald_lsp", {
  cmd = { vim.fn.expand("~/repos/skald/build/skald_lsp") },
  filetypes = { "skald" },
  root_markers = { ".git" },
})
vim.lsp.enable("skald_lsp")

```

If Neovim doesn't recognize `.ska` as filetype `skald`, add:

```lua
vim.filetype.add({
  extension = {
    ska = "skald",
  },
})
```

### Updating After Local Changes

The LSP binary is self-contained -- Neovim launches it as a subprocess each time you open a `.ska` file. After rebuilding with `cmake --build build --target skald_lsp`, just restart Neovim to get the fresh LSP.

## Manual Testing

The LSP uses JSON-RPC over stdin/stdout with `Content-Length` header framing (not raw JSON). To send a request manually:

```bash
printf 'Content-Length: 75\r\n\r\n{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}}' | ./build/skald_lsp
```

The wire format is:

```
Content-Length: <byte count of JSON body>\r\n
\r\n
<JSON body>
```

To send multiple messages in sequence, concatenate them:

```bash
INIT='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}}'
OPEN='{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///tmp/test.ska","languageId":"skald","version":1,"text":"#start\nHello world.\n-> end\n"}}}'
{
  printf "Content-Length: ${#INIT}\r\n\r\n%s" "$INIT"
  printf "Content-Length: ${#OPEN}\r\n\r\n%s" "$OPEN"
} | ./build/skald_lsp
```

NOTE: Don't send raw JSON, because the transport layer expects `Content-Length` headers first!

## Capabilities

| Feature | Trigger |
|---|---|
| **Diagnostics** | Automatic on open/edit. Syntax errors, undefined block tags (`-> missing`), unused variables. |
| **Go to Definition** | `gd` or equivalent. Jumps from `-> tag` to `#tag`, from variable references to `~ var = ...` declarations, from `GO file.ska` to that file. |
| **Find References** | Shows all occurrences of a block tag, variable, or method. |
| **Completion** | After `-> `: block tags. After `:`: methods. Inside `{...}` or `(? ...)`: variables. After `GO `: `.ska` file paths from the workspace. |
| **Hover** | Variable type and initial value, block beat count, method/file ref info. |
| **Document Symbols** | Outline view showing declared variables and block tags. |

## Architecture

- Everything comes into `main.cpp` ...
- ... which starts up a Server (`server.h/cpp`).
- The key processing area is `lsp_actions.h`.
- This parses into `LspParseState` (`lsp_parse_state.h`) which tracks symbols and their locations.
- The analyzer in `analyzer.h/cpp` sets up autocompletion etc.

The LSP reuses the Skald engine's PEGTL grammar (`skald_grammar.h`), parse actions (`skald_actions.h`), parse state (`parse_state.h`), and AST types (`skald.h`) by linking against `skald_static`. The key extension point is `lsp_action<Rule>`, which inherits from the base `action<Rule>` so all AST-building logic runs unchanged, then layers symbol-position tracking on top.

Parsing uses `pegtl::memory_input` (string buffers from the editor) instead of `pegtl::file_input`. The full file is re-parsed on every keystroke -- Skald files are small and PEGTL is fast.

### File Responsibilities

```
lsp/src/
  main.cpp              Entry point. Stdin/stdout JSON-RPC loop.
  transport.h/cpp       Content-Length framed message reading/writing.
  lsp_types.h           LSP protocol structs (Position, Range, Diagnostic,
                        CompletionItem, DocumentSymbol, etc.) with
                        nlohmann/json serialization.
  server.h/cpp          Method dispatch. Routes initialize, didOpen, didChange,
                        definition, references, completion, hover,
                        documentSymbol, shutdown, exit. Owns the document map
                        and workspace.
  document.h/cpp        Per-document state. Holds the text buffer, parses it
                        into a Module + symbol list, and runs semantic checks
                        (undefined tags, unused variables). This is the core
                        unit -- most new diagnostics or analyses go here.
  analyzer.h/cpp        Stateless helpers. Completion context detection,
                        document symbol building, hover text generation.
  workspace.h/cpp       Workspace-level state. Discovers .ska files under the
                        root for GO-path completion. Handles URI/path conversion.
  lsp_parse_state.h     LspParseState, extending Skald::ParseState with a
                        vector of SymbolOccurrence (name, kind, position,
                        is_definition) and scratch ranges for the action layer.
  lsp_actions.h         lsp_action<Rule> template specializations. Each override
                        records symbol positions before/after calling the base
                        Skald::action. This is where you add tracking for new
                        grammar rules.
```

### How Symbols Are Tracked

`lsp_action<Rule>` overrides fire during PEGTL parsing. Each override:

1. Captures the identifier name and source range from the PEGTL input
2. Records a `SymbolOccurrence` (name, kind, is_definition, range) into `LspParseState::symbols`
3. Calls the base `Skald::action<Rule>` to run the normal AST-building logic

The key subtlety: PEGTL does *not* fire `action<identifier>` when matching through an inheriting rule like `block_tag_name : identifier` or `r_variable : variable_name : identifier`. So each alias rule that needs position tracking gets its own `lsp_action` specialization that computes the range directly from `input.position()` and `input.size()`.

For mutation operations (`~ var += ...`), the lvalue identifier's range is saved by `lsp_action<op_mutate_start>` before the rvalue is parsed, since rvalue parsing can overwrite `last_identifier_range`.

### Adding Support for New Grammar Rules

To track a new symbol-bearing rule:

1. Add a `template <> struct lsp_action<Skald::your_rule>` specialization in `lsp_actions.h`
2. Compute the range with `range_from_input(input)` or read `state.last_identifier_range`
3. Call `state.record_symbol(name, kind, is_definition, range)`
4. Call `Skald::action<Skald::your_rule>::apply(input, state)` (or `apply0`) to run the base logic
5. Add any new semantic checks in `Document::run_semantic_checks()` in `document.cpp`

### Adding New LSP Methods

1. Add the handler method to `Server` in `server.h`
2. Implement it in `server.cpp`
3. Wire it into `Server::handle_message()` (requests have an `id`, notifications don't)
4. Advertise the capability in `handle_initialize()`
