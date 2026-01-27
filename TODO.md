# Skald TODO

## This Milestone

- [x] Boolean ternaries with truthy values
- [x] Make sure logic beats
- [x] Test complex conditionals
- [ ] Else logic beats

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
