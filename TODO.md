# Skald TODO

## CURRENT

- [ ] 1. Set cursor in `start` methods
- [ ] 2. Pre-scan the beat for resolvers and add to the cursor stack
- [ ] 3. Advance cursor with next here
- [ ] 4. Return response of either Content or Query (if stack is not empty)
- [ ] 5. Implement the answer method and advance the cursor
- [ ]   - Set up a simple cin answer in the test module
- [ ] 6. Implement the answer cache
- [ ] 7 . Finish the resolver

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

### Smart Tags

You can do something like the following:

```
Oh yes, of course I have heard of the [Imperium], also known as the [Imperium|Empire of Man]!
```

Where this will mark something as a "smart tag" (with an optional label right of the bar), that can be read by the parser for special text elements.


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
