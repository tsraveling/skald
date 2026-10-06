# Migrating to Logic Consolidation

This release unifies method calls and mutations under a single `~` logic prefix, drops the `:` method prefix everywhere, and changes conditional equality from `=` to `==`. Only `.ska` source files are affected; `.codex` files need no changes.

## What changed

| Construct | Before | After |
|---|---|---|
| Method call (statement) | `:do_thing(1)` | `~ do_thing(1)` |
| Guarded method call | `(? flag) :do_thing()` | `(? flag) ~ do_thing()` |
| Method as mutation value | `~ x = :get_value()` | `~ x = get_value()` |
| Method in a conditional | `(? :check())` / `(? !:check())` | `(? check())` / `(? !check())` |
| Method in `@if` / `@elseif` | `@if :check()` | `@if check()` |
| Method in an injection | `{:get_name()}` | `{get_name()}` |
| Equality in a conditional | `(? x = 1)` | `(? x == 1)` |
| Equality in `@if` / `@elseif` | `@elseif x = 1` | `@elseif x == 1` |

Also new:

- Whitespace after `~` is optional: `~do_thing()` and `~x += 1` are valid.
- Methods and variables can be mixed freely in conditionals: `(? check() and flag)`.

Unchanged:

- Mutations still use a single `=`: `~ x = 5`, `~ x += 1`, `~ x -= 1`, `~ flag =!`.
- `!=`, `>`, `<`, `>=`, `<=` are unchanged.
- `@let`, `@testbed`, and codex declarations still use a single `=` for defaults.
- Attributions (`alice: Hello`), ternaries (`{a ? "x" : "y"}`), and switch ternaries (`{v ? [1: "a"]}`) still use `:`.
- Compiled Lua / JSON output and the engine API are identical; no consumer changes are needed.
- Using a method as an argument to another method (`~ a(b())`) is still unsupported, but it is now reported as an error.

## Why this matters: silent failures

Two kinds of unmigrated lines do **not** produce a compiler error:

- `:do_thing()` on its own line parses as a plain beat, so the text `:do_thing()` is shown to the player and the method never runs.
- `(? x = 1) Some text` fails to parse as a conditional, so the whole line, including `(? x = 1)`, becomes beat text.

`@if x = 1` / `@elseif x = 1` do error ("Malformed line"). The LSP flags `=`, `=>`, and `=<` inside any condition, but does not flag a leftover `:` method. Migrate every file; don't rely on the compiler to find them.

## Migration steps

1. Commit or back up your `.ska` files so the migration is easy to review as a diff.
2. Save the script below as `migrate.pl` and run it over every module:
   ```sh
   find path/to/project -name '*.ska' -exec perl -i migrate.pl {} +
   ```
   It edits files in place and is safe to run more than once.
3. Review the diff (`git diff`). Check the cases under **Known limitations**.
4. Compile the project and fix anything reported:
   ```sh
   skalder --compile lua path/to/project
   ```
5. Search for anything the script missed. Neither of these should match after migrating (other than inside quoted strings or prose):
   ```sh
   grep -rnE '(^|[^[:alnum:]_"]):[A-Za-z_][A-Za-z0-9_]*\(' --include='*.ska' path/to/project
   grep -rnE '(\(\?|@if|@elseif)[^)]*[^=!<>]=[^=<>]' --include='*.ska' path/to/project
   ```
   The second pattern can also match a mutation after a guard (`(? flag) ~ x = 1`); those are correct and can be ignored.

## Migration script

```perl
#!/usr/bin/env perl
# Migrates .ska files to the logic-consolidation syntax, in place.
# Usage: perl -i migrate.pl path/to/*.ska
use strict; use warnings;

# Replaces lone `=` with `==` in a condition, skipping string literals.
sub fix_eq {
  my ($s) = @_;
  my @parts = split /("(?:[^"\\]|\\.)*")/, $s;
  for my $i (0 .. $#parts) {
    next if $i % 2;
    $parts[$i] =~ s/(?<![=!<>])=(?![=<>])/==/g;
  }
  return join '', @parts;
}

while (my $line = <>) {
  # 1. Method statements: `:m(...)` -> `~ m(...)`, keeping indent and `(? ...)`.
  $line =~ s/^(\s*(?:\(\?.*?\)\s*)?):(?=[A-Za-z_]\w*\()/$1~ /;

  # 2. Methods as values: drop `:` before `name(`.
  $line =~ s/(?<![\w"]):(?=[A-Za-z_]\w*\()//g;

  # 3. Equality in conditions: `=` -> `==`.
  my ($code, $comment) = $line =~ /^(.*?)((?:---.*)?\n?)$/s;
  if ($code =~ /^(\s*\@(?:if|elseif)\s)(.*)$/s) {
    $code = $1 . fix_eq($2);
  } else {
    my ($out, $last) = ('', 0);
    while ($code =~ /\(\?/g) {
      my $start = pos($code);
      my ($depth, $i) = (1, $start);
      while ($i < length($code) && $depth > 0) {
        my $c = substr($code, $i, 1);
        $depth++ if $c eq '(';
        $depth-- if $c eq ')';
        $i++;
      }
      $out .= substr($code, $last, $start - $last)
            . fix_eq(substr($code, $start, $i - $start));
      $last = $i;
      pos($code) = $i;
    }
    $code = $out . substr($code, $last);
  }
  print $code . $comment;
}
```

## Known limitations of the script

- **Ternary with no space before a method**: `{flag ? "a" :get_b()}` loses its `:` and breaks. Write ternaries with a space (`: get_b()`) before migrating, or fix by hand.
- **Parentheses inside strings in a condition**: `(? s = ")")` can confuse the condition's end. Rare; fix by hand.
- **`(?` in plain beat text** is treated as a condition, and any `=` after it becomes `==`.
- **`=>` / `=<`** (invalid before and after) are left alone; change them to `>=` / `<=` by hand. The LSP flags them.

## Manual migration checklist

If you're migrating by hand instead, or reviewing the script's output, check each `.ska` file for:

- [ ] Every line that starts with `:name(` (optionally indented or after a `(? ...)` guard) now starts with `~ name(`.
- [ ] No `:` remains directly before a method name in mutation values, conditionals, `@if` / `@elseif`, injections, or method arguments.
- [ ] Every equality check inside `(? ...)`, `@if`, and `@elseif` uses `==`.
- [ ] Mutations (`~ x = ...`) and declarations (`@let`, `@testbed`) still use a single `=`.
- [ ] The project compiles cleanly with `skalder --compile`.
