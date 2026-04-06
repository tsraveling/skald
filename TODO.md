# Skald TODO

## New conditional syntax

```
@if condition

Condition is true!

@elseif sky="red"

Seems apocalyptic
> Continue
> Flee

@else

Everything seems fine!

@end

(? check) this is still a one-liner.
```

1. @if is always followed by a conditional.
2. For single line conditionals, use the inline syntax (`(? check)`)
3. everything after the conditional line is conditioned until @elseif, @else, or @end.
4. @elseif only fires if the previous statement did not, and then if its conditions are met.
5. @else always fires if the previous statement did not,
6. @end exits the conditional stack.

- No conditional nesting.

## Sagas: Global, thread, and local state

1. A Skald project should have at minimum one .saga file, e.g. `main.saga`.
2. A saga file owns all .ska files in its own folder and subfolders.
    - If a subfolder contains another saga file, or if there are more than one in a given directory, a warning or error will be thrown.
    - We call the saga a .ska file belongs to the "mother saga".
3. The saga file defines methods and globals:

```
-- main.saga

-- Contains the main game stories.

@methods
  -- Will get added to hover text
  -- a: describe a param
  call_something(a string) int

  do_something() action -- action types do not return a value.
@end

@globals
  -- hover text here too
  @readonly player_name string = "Test"

  -- will be zero default if not defaulted
  player_level int
@end

-- You can set a custom output folder for annotations documents
@annotations = ~/notes/annotations

```

4. **Globals** are injected into Skald as arguments with each action. They reflect values stored on the game side.
5. **Methods** are defined here and can be called with `:call_method()` syntax directly or as rvalues (if not action typed).
6. All paths e.g. `GO beach/fire.ska` are relative to the mother saga.
7. **Ad hoc variables** are not declared, just used directly inline. If not initialized, they will be zeroed. Ad hoc variables are not strongly typed.
8. **Declared variables** are typed, and declared in a .ska file like this:
    ```
    ~ some_value int = 6
    ```
9. Declared variables are **pushed** on GO. Pushed variables are automatically picked up by the LSP, but you can inform the LSP that you expect a push from a file in the future like this:
    ```
    > beach/fire.ska -- will make declared vars from here available in LSP locally.
    ```

### Subsections

1. There is now a space between # and tag name.
2. Children are `## child`.
3. Grandchildren are `### grandchild`, and so on.
4. Transitions are now contextual:
    - `-> tag` goes to a top level tag, as before
    - `-> .child_tag` goes to some child of the current block
    - `-> -sib_tag` goes to some sibling of the current block
    - `-> ^` goes to the parent
    - `-> ^-tag` goes to an aunt
    - `-> ^.tag` goes to a cousin
    ` -> some_tag.child.grandchild` goes to a specific descendant of another tag;
    - As `-> .child.grandchild` does of this one.

### Subtasks

- [ ] Codegen


## Typing

## Skalder Fixes:

- [x] Add choices to output log
- [x] For queries that do not need responses, add a new NarrativeItem type and do that instead
    - [x] By doing a standardized `setup_response(Response &r)` method that handles those and returns anything that's not that.
- [x] Add error handling using new NarrativeItem struct on TODOs
- [x] Fix query inputs
- [ ] Redirect not found (specifically from the "olympics joke" choice) causes a hard crash

## MSVC / Cross-platform

- [ ] Replace `uint` with `unsigned int` in `skald.h` — `uint` is a POSIX typedef and doesn't exist on MSVC
- [ ] Implement `Engine::get_current()` — declared in `skald.h` but never defined

## Soon

- [ ] Inline logic beats
- [ ] Inline choices
- [ ] Addition and subtraction mutations
- [ ] Testbeds
- [ ] Module transitions

## Future Features


### Chance Insertions

We should hold off on these because it seems like their primary use will be PCG stuff, and it is decently likely there will be a better way to do that. If that decision is finalized we will need to update the syntax docs accordingly.

- [ ] 2.3.4 Chance Ternaries
    - [ ] Chance Switch Ternaries
    - [ ] Weights
    - [ ] Conditional options
- [ ] 2.5 Chance blocks
    - [ ] Weighting
    - [ ] With logic beats

#### Add to Syntax.md:

-- ### 2.3.4 Chance Ternaries

You can use a **chance ternary** to inject an option at random:

```
I prefer {% ? "dark" : "light"} roast coffee.
```

You can also make a **chance switch ternary**:

```
Her pet is a {% ? ["dog", "cat", "donkey"]} -- these choices are evenly weighted
```

You can weight options like this:

```
Her pet is a {% ? ["dog", 3:"cat", 5:"donkey"]} -- This pet has a 5/9 chance of being a donkey
```

Finally, you can mark some options as conditional with paren syntax:

```
Her pet is a {% ? ["dog", (?is_cat_person):"cat", 3(?is_pirate): "parrot"]}.
```

In this example, if `is_pirate` is not set but `is_cat_person` is, it's a coin-flip between dog and cat. If she is a pirate, however, the pet has a 3/5 chance of being a parrot. If neither flag is set, it will always be "dog".


### Smart Tags

You can do something like the following:

```
Oh yes, of course I have heard of the [Imperium], also known as the [Imperium|Empire of Man]!
```

Where this will mark something as a "smart tag" (with an optional label right of the bar), that can be read by the parser for special text elements.

--- 2.5 Chance blocks:

If successive blocks are prefixed with (%), the engine will pick one to present at random:

```
(%) Hi there!
(%) Why hello.
(%) Sup?
```

You can mark some chances as conditional. Conditionals will only be included in the "draw pile" if the conditional is met:

```
(%) Hello, stranger.
(%) Greetings!
(%? has_met) Nice to see you again.
```

Chance blocks can be weighted with an integer prefix:

```
(2%) Ahoy!      -- This has a 2/3 chance of printing
(%) Hoy!        -- This has a 1/3 chance of printing
```

As with any other kind of block, logic can be added:

```
You roll the dice.

(5%) You miss, and pay up.
    ~ money -= 100
(%) You hit six!
    ~ money += 300
```

This also works with logic blocks (`*`):

```
(%) * -> find_way
(3%) * -> get_lost
```


## Next Steps
    
Writing code:

- [ ] Syntax parser
- [ ] Treesitter
- [ ] LSP

Actual usage:

- [ ] Core engine
- [ ] Godot plugin
- [ ] Unreal plugin?

Ancillary Tools:

- [ ] Terminal tester
- [ ] Godot window-based tester


# C++ Notes

- std::exchange(conditional_buffer, std::nullopt)

# PEGTL Notes

## Patterns

- seq: linear sequence of children
- star: zero or more of
- plus: one or more of
- one: one of

## Symbols

- blank: whitespace
- eol: end of line
- identifier_other: sequence that can be used as an identifier


## Considerations

- [ ] String handling in ternaries etc needs clarification
- [ ] We have to figure out a good way of handling localization

# Future Features

- [ ] Define an interface of methods we can use up front

# Edge Cases

- [ ] Methods as arguments of other methods
