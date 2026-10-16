## FIXES

- LSP: parser breaks when typing a `(? variable >= 3)` type of conditional. Boolean ones seem to work fine.
- skalder crashes on go with dest block
- method call syntax doesn't really make sense in conditionals. maybe better to just prefix all commands with ~, e.g. ~ index ++ or ~ call_method()?
- Parser: inline choice redirect `> text -> a.b` builds its Move from `pop_id()` (skald_actions.h `inline_choice_move`), so a dotted or relative target likely keeps only the last identifier instead of `move_identifier_store`.
- Parser: switch ternary default `_` (`switch_default`) has no action, so `{x ? [1:"a", _:"b"]}` pops the wrong rvals.
- Parser: `@receive` matches in the grammar but has no action and no `Module` field; it is silently dropped.
- Parser: `EXIT some_var` errors because `Exit.argument` is `SimpleRValue`; Syntax.md 4.1.2 says variables are allowed.
- Codex: `@readonly` documented in Syntax.md 4.2.2 but absent from `codex_grammar.h`.
- Parser: `argument_queue` leaks on a failed method call (half-typed `foo(1,`); arg actions push eagerly (skald_actions.h:307) and only a completed call drains it, so the next call inherits stray args.


## NEXT


## TO DO


## DONE

