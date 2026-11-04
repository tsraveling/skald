# Overview and Usage

The core of Skald is an **engine** that optionally sets up a **codex** and then loads a **module**, which is a `.ska` file.

With a Skald module loaded:

1. `.start()` or `.start_at("tag")` returns the first **Response**.
2. A Response can be **Content**, **Query**, or **End**.
3. Respond:
  - A **Content** is something you can handle and render: a piece of dialogue, a set of options (an `OptionGroup`), an action notification, etc. If it's an option group, you call `act(index)` with the option your player picks in order to get the next Response.
  - A **Query** means Skald needs something from your game -- a calculation from game state, a minigame result, whatever you want. You respond to it with `.answer` (see below). This is entirely asynchronous, which means you could do `@if win_chess()` in a module, have an entire ten minute chess minigame, and then advance the Skald module from there.

# API Description

## Engine

`Skald::Engine` is the main entry point: it loads a codex and modules, walks the script, and hands you `Response` objects that your client reacts to.

An `engine` loads a single codex file and uses that for everything. If your project has multiple codices, you would have multiple instances of `engine`.

An engine can run in "no codex" mode, which means no globals, no methods, and module paths are as-is, not relative to the codex.

### .setup(codex_path)

Loads a `.codex` file (globals, method definitions, project root) and initializes engine state. Returns a `ParseResult` describing success and any parse errors. `::load` once this is set will load relative to that codex root. So if codex is `/content/game.godex`, and a module lives at `content/jungle/entry.ska`, then `GO jungle/entry.ska` will work.

- `path` (`std::string`): path to the codex file.

```cpp
Skald::Engine engine;
auto result = engine.setup("game/main.codex");
if (!result.ok) {
  for (auto &err : result.exceptions)
    std::cerr << err.msg << "\n";
}
```

### .load(module_path)

Parses and loads a Skald module (`.ska` file), setting it as current. Returns a `ParseResult`. Relative to codex if the engine loaded one; full path if not.

- `path` (`std::string`): path to the module file.

```cpp
auto result = engine.load("chapters/intro.ska"); // resolved against codex root
if (!result.ok) { /* report result.exceptions */ }
```

### .set_source_reader(reader_callback)

**Optional.** Overrides how raw file content is fetched, letting embedders with a virtual filesystem (Godot `res://`, archives, network, in-memory) supply bytes. If this isn't set, the default reads from the OS filesystem via `default_source_reader`.

The source reader just needs to return text with codex or module content for a given path. You can use this for specific filesystems, binary data, encryption, or even a remote server -- whatever you want.

- `reader` (`SourceReader`): callback that takes a resolved path and returns the file's text, or `std::nullopt` if it can't be read.

```cpp
engine.set_source_reader([](const std::string &path) -> std::optional<std::string> {
  auto file = my_vfs::open(path); // e.g. Godot res://, archive, network
  if (!file) return std::nullopt;
  return file->read_all_text();
});
```

### .start()

Module must be loaded (via `.load`) first!

Starts execution at the first block in the module, setting the cursor to its first beat. Returns the first `Response`.

```cpp
Skald::Response response = engine.start();
handle(response); // this method is one you write to handle Responses -- see below.
```

### .start_at(tag)

Starts execution at the block with the given tag, setting the cursor to its first beat. Returns the first `Response`.

- `tag` (`std::string`): tag of the block to start in.

```cpp
Skald::Response response = engine.start_at("tavern_entrance");
handle(response);
```

### .act(index)

Acts on `Content` or `OptionGroup` response and advances the engine. This is used to do a "next" action on Content, or to pick a choice on an option group. Returns the next `Response`.

- `choice_index` (`int`, default `0`): index of the selected choice when choices are presented. Use `act()` on Content (though any integer will work the same as the default 0 in that case).

```cpp
// Plain content: just advance.
auto next = engine.act();

// OptionGroup: pass the index of the player's pick.
auto after_choice = engine.act(2);
```

### .answer(query_answer)

Answers an open query (`MethodCallGet`) so the engine can proceed. Must be of the required type, which will be defined in the codex: an int, a float, a string, or a boolean. Returns the next `Response`. An **Error** will be return on a type mismatch.

- `answer` (`std::optional<QueryAnswer>`): the value your method returned, or empty/nullopt if no value is returned.

```cpp
// Engine asked us to run player_gold() -> int; reply with the value:
auto next = engine.answer(Skald::QueryAnswer{.val = 120});

// Method returns nothing:
auto after_post = engine.answer(std::nullopt);
```

### .get_current()

Returns the `Response` currently awaiting client action, without advancing.

```cpp
Skald::Response current = engine.get_current();
if (Skald::get_response_type(current) == Skald::ResponseType::QUERY) {
  // still waiting on engine.answer(...)
}
```

### .set(key, val)

Sets a global variable; errors if the global doesn't exist or the type mismatches. Returns `std::nullopt` on success, or an `Error`.

- `key` (`std::string`): global variable name.
- `val` (`SimpleRValue`): value to assign.

```cpp
if (auto err = engine.set("player_name", std::string("Astrid"))) {
  std::cerr << err->message << "\n";
}
engine.set("gold", 50);
```

### .get(key)

Reads a global variable. Returns the `SimpleRValue`, or an `Error` if the variable isn't set.

- `key` (`std::string`): global variable name.

```cpp
auto result = engine.get("gold");
if (auto *val = std::get_if<Skald::SimpleRValue>(&result)) {
  int gold = *Skald::srval_get_int(*val);
}
```

### .get_project_root()

Returns the codex's project root directory, or `std::nullopt` if no codex is loaded.

```cpp
if (auto root = engine.get_project_root()) {
  std::cout << "Project root: " << *root << "\n";
}
```

### .get_codex_name()

Returns the loaded codex's filename, or `std::nullopt` if no codex is loaded.

```cpp
if (auto name = engine.get_codex_name()) {
  std::cout << "Codex: " << *name << "\n";
}
```


## Static methods

### ::default_source_reader(path)

The default `SourceReader`: reads a file from the OS filesystem. Returns the file's text, or `std::nullopt` if it can't be found or read. If you don't set a source reader, this one will be used automatically, so you will likely rarely if ever need to use this method directly.

- `path` (`std::string`): resolved path to read.

```cpp
if (auto text = Skald::default_source_reader("game/intro.ska")) {
  std::cout << *text;
}
```

### ::get_response_type(response)

Classifies a `Response` into a `ResponseType` so you can dispatch without a manual `std::visit`.

- `response` (`Response&`): the response to inspect.

```cpp
switch (Skald::get_response_type(response)) {
case Skald::ResponseType::CONTENT:
  show(std::get<Skald::Content>(response));
  break;
case Skald::ResponseType::QUERY:
  run_query(std::get<Skald::MethodCallGet>(response));
  break;
case Skald::ResponseType::END:
  finish();
  break;
default:
  break;
}
```

## SimpleRValue helpers

All values on `Response` types, e.g. method arguments, Exit values, etc., will come in as `SimpleRValue`, a variant of type `int`, `float`, `std::string`, or `bool`. The following take a `SimpleRValue` as an arg and return the relevant value:

- `srval_get_str` returns `*std::string`
- `srval_get_int` returns `*int`
- `srval_get_float` returns `*float`
- `srval_get_bool` returns `*bool`

These helper methods will return `nullptr` if the type is incorrect.

```cpp
if (const int *n = Skald::srval_get_int(srval)) {
    do_something_with(*n);
}
```

# Debug Functions

The following methods and types are only used in debugging, or when setting up debug features in your own game or integration.

## Engine Debug Methods

The following methods are available, but mostly used for debugging:

### .trace()

Debug usage only. Utility that parses and walks a module at the given path, printing trace output.

- `path` (`std::string`): path to the module file.

```cpp
engine.trace("chapters/intro.ska"); // dumps grammar trace to stdout
```

### .dbg_print_cache()

Debug utility returning the query answer cache as a printable string. Useful for building debug tools in your game.

```cpp
std::cout << engine.dbg_print_cache();
```

## RValues

An `RValue` is a value in Skald *before* it has been resolved. It has the same four base types as SimpleRValue (int, bool, string, float), and in addition can support **methods** and **variables**. Variables are pulled from global, module, or local scope (in that order); methods must first be resolved using the `QueryAnswer` system.

As such, `Response` types always resolve before sending. If you want to try to dig into the inner workings of Skald, though, or extend state-watching capabilities, here are some useful methods.

### Skald::cast_rval_to_simple(rval)

Narrows an `RValue` to a `SimpleRValue`, returning `std::nullopt` when the value is a `Variable` or `MethodCall` and can't be represented simply.

- `rval` (`const RValue&`): value to cast.

```cpp
if (auto simple = Skald::cast_rval_to_simple(rval)) {
  store(*simple);
} else {
  // holds a Variable or MethodCall; needs engine resolution
}
```

### Skald::is_simple_rval_truthy()

Returns the truthiness of a `SimpleRValue`: non-empty for strings, non-zero/true otherwise. Used for conditionals and ternaries.

- `val` (`const SimpleRValue&`): value to test.

```cpp
Skald::SimpleRValue v = std::string("hello");
bool truthy = Skald::is_simple_rval_truthy(v); // true (non-empty string)
```

### Skald::get_zero(value_type)

Returns the zero value for a `ValueType` (`0`, `0.0f`, `""`, or `false`).

- `t` (`ValueType`): type to zero.

```cpp
Skald::SimpleRValue blank = Skald::get_zero(Skald::ValueType::FLOAT); // 0.0f
```

### Skald::val_type_to_str(value_type) / Skald::scope_to_str(var_scope)

`val_type_to_str` renders a `ValueType` as its lowercase name; `scope_to_str` does the same for a `VarScope`.

```cpp
Skald::val_type_to_str(Skald::ValueType::INT);   // "int"
Skald::scope_to_str(Skald::VarScope::MODULE);    // "module"
```

### Skald::rval_to_string(rval)

Template rendering any `RValue`/`SimpleRValue` variant to a display string, mainly for debugging.

- `val` (variant): the value to stringify.

```cpp
std::cout << Skald::rval_to_string(rval);   // e.g. "42", "{T}", "gold"
```

### Skald::key_for_call(method_call)

Builds the cache key string used to store a method call's answer, from the method name and its arguments. That answer is then used during the resolution of the `RValue` that called the method into the `SimpleRValue` that the client returned in its `QueryAnswer`.

- `call` (`MethodCall&`): the call to encode.

```cpp
Skald::MethodCallGet &query = std::get<Skald::MethodCallGet>(response);
std::string key = Skald::key_for_call(query.call); // e.g. "roll_dice|6"
```

# Type Glossary

## Type aliases

`SourceReader` is `std::function<std::optional<std::string>(const std::string&)>`: given a resolved path, return the module text or `std::nullopt`.

`RValue` is `std::variant<std::string, bool, int, float, Variable, std::shared_ptr<MethodCall>>`: any right-hand value appearing in a script.

`SimpleRValue` is `std::variant<std::string, bool, int, float>`: a fully-resolved literal value, the currency of state get/set and query answers.

`Response` is `std::variant<Content, MethodCallGet, MethodCallPost, Exit, GoModule, OptionGroup, End, Error, Notification>`: everything the engine can hand back to the client.

`MemberBody` is `std::variant<Move, MethodCall, Mutation, GoModule, Exit, Beat>`: the raw operation types a `Member` can hold.

`BlockMember` is `std::variant<Member, ChoiceGroup>`; `MainBlockMember` is `std::variant<BlockMember, ConditionalChain>`; `mbm_is_chain` tests the latter for a chain.

`ConditionalItem` is `std::variant<ConditionalAtom, std::shared_ptr<Conditional>>`, letting conditions nest. `dbg_desc_conditional_item` renders one for debugging.

`TextPart` is `std::variant<std::string, SimpleInsertion, TernaryInsertion>`; `TernaryOption` is `std::tuple<RValue, RValue>`.

## Enums

`ValueType` (`STRING`, `BOOL`, `INT`, `FLOAT`, `ACTION`) strongly types declarations and methods; only method definitions are ever `ACTION`.

`VarScope` (`GLOBAL`, `MODULE`, `LOCAL`) identifies which state layer a variable lives in.

`SkaldLogLevel` (`VERBOSE`, `NORMAL`, `SPARSE`, `OFF`) controls library logging via the inline global `Skald::log_level`.

`ResponseType` (`CONTENT`, `QUERY`, `EXIT`, `GO_MODULE`, `END`, `ERROR`, `UNKNOWN`) classifies a `Response` via `get_response_type`.

`ParseError::Severity` (`WARNING`, `ERROR`) grades parse diagnostics.

`ConditionalAtom::Comparison` (`TRUTHY`, `NOT_TRUTHY`, `EQUALS`, `NOT_EQUALS`, `MORE`, `LESS`, `MORE_EQUAL`, `LESS_EQUAL`) is the comparison operator of one condition atom; `comparison_for_operator` maps an operator string (`=`, `!=`, `>`, ...) to it.

`Conditional::Type` (`AND`, `OR`) joins the items of a compound condition.

`Mutation::Type` (`EQUATE`, `SWITCH`, `ADD`, `SUBTRACT`) is the kind of variable mutation; `Mutation::label_for_type` renders it as a label.

`ProgressResult` (`OK`, `END_OF_FILE`, `MODULE_NOT_FOUND`) reports low-level cursor progress outcomes.

## Response payload types

`Content` is a resolved narrative beat: `attribution` (speaker, may be empty) and `text` (a `std::vector<Chunk>`). `Chunk` wraps one `text` string.

`OptionGroup` presents choices: `options` is a `std::vector<Option>`, each with resolved `text` chunks and an `is_available` flag; answer with `Engine::act(index)`.

`MethodCallGet` asks the client to run a method and return a value: `call` (the `MethodCall`), `line_number`, and `get_key()` which returns the caching key. Respond via `Engine::answer`.

`MethodCallPost` is a fire-and-forget method call to the client: `call` and `line_number`, no return expected.

`QueryAnswer` carries the client's reply to a query: `val` is an optional `SimpleRValue`, and an empty value is treated as falsy/unset.

`Notification` reports a state mutation to the client: `var_name`, `mut_type` (`Mutation::Type`), resolved `rval`, and `scope`.

`GoModule` signals a transition to another module: `module_path` plus optional `start_in_tag`.

`Exit` signals script exit with an optional `argument` SimpleRValue.

`End` is an empty terminator meaning the script concluded, with an optional debug `reason`.

`Error` is a fatal engine error: `code` (one of the `ERROR_*` constants below), `message`, and `line_number`.

`Warning` is a non-breaking issue logged internally: `message` and `line_number`.

## Error codes

Constants `ERROR_UNKNOWN` (0), `ERROR_EOF` (1), `ERROR_EMPTY_MODULE` (2), `ERROR_MODULE_TAG_NOT_FOUND` (3), `ERROR_CHOICE_OUT_OF_BOUNDS` (4), `ERROR_CHOICE_UNAVAILABLE` (5), `ERROR_EXPECTED_ANSWER` (6), `ERROR_RESOLUTION_QUEUE_EMPTY` (7), `ERROR_TYPE_MISMATCH` (8), `ERROR_UNEXPECTED_NULL` (9), `ERROR_VAR_UNDEFINED` (10), `ERROR_UNEXPECTED_ACT` (11), `ERROR_LOADING_MODULE` (12), `ERROR_NO_GLOBAL` (13), `ERROR_OUT_OF_BOUNDS` (14), `ERROR_START_EMPTY_BLOCK` (15) populate `Error::code`.

## Parsing types

`ParseResult` reports the outcome of `setup`/`load`: `ok` is false if any exception is error-severity, and `exceptions` lists all `ParseError`s. Static helpers `with(exceptions)` and `fail(msg)` construct results.

`ParseError` is one parse diagnostic: `pos` (a `ParsePosition`), `msg`, and `severity`. `ParseError::file_error(msg)` builds a file-level error with no position.

`ParsePosition` locates a diagnostic: `line`, `column`, and `source` (the file or codex it came from).

## Script model types

These are the parsed AST structures; you mostly meet them inside `Response` payloads or when inspecting a loaded `Codex`/`Module`. Most derive from `LineEntity`, which contributes a `line_number` field, and carry a `dbg_desc()` debug string.

`Codex` is the project definition: `path` and `filename`, `global_vars` (declared globals), and `method_defs` (client method signatures). `codex_path()` returns the full codex file path, and `resolve_path(rel)` resolves a module path against the codex directory.

`Module` is one parsed Skald file: `filename`, `module_vars`, `testbeds`, `blocks`, and a `block_lookup` map. `get_block_index(tag)` returns the block index for a tag, or `-1`.

`Block` is a tagged section of a module: `tag` and `members` (a list of `MainBlockMember`).

`Variable` is a typed variable reference: `name` and `type`. `DeclaredVar` pairs a `var` with its `initial_value`.

`MethodDef` declares a client method: `name`, `return_type`, and `args` (a list of `ArgDef`). `ArgDef` is one parameter: `name` and `type`.

`MethodCall` is an invocation of a client method: `method` name and `args` (a list of `RValue`, which may include nested calls).

`ConditionalAtom` is one comparison: left operand `a`, a `Comparison`, and optional right operand `b`. `Conditional` combines `items` with an `AND`/`OR` `type`; `AttachedCondition` wraps an optional `Conditional` and is truthy when a condition is present.

`ConditionalBlock` is one branch of an if/elseif/else chain: a `cond` (empty means else) and its `members`. `ConditionalChain` is the whole chain as `cond_blocks`.

`Member` is one executable line: `body` (a `MemberBody`) plus an `AttachedCondition` `ac`. Typed accessors `get_move/get_call/get_mutation/get_go_module/get_exit/get_beat` return pointers or `nullptr`, with matching `is_*` predicates.

`Beat` is a narrative text line: optional `attribution` and `content` (a `TextContent`).

`Move` jumps to another block via `target_tag`. `Mutation` changes a variable: `lvalue`, a `Type`, and optional `rvalue`.

`Choice` is one selectable option: an availability `condition`, display `content`, and the `members` executed when picked. `ChoiceGroup` holds one or more `choices` and blocks progress until one is selected.

`TextContent` is interpolated text as a list of `TextPart`s. `SimpleInsertion` splices one `rvalue`; `TernaryInsertion` picks among `options` based on a `check` value; `ChanceInsertion`/`ChanceOption` pick a value by random `weight`.

`Testbed` is a named set of test variable assignments: `name` and `declarations`. `TestbedSet` is one assignment: `variable` and `test_value`.

`Cursor` tracks engine position and pending client obligations (queued transitions, exits, and the query `resolution_stack`); it's exposed but managed by the engine, and `reset()` returns it to a fresh state.
