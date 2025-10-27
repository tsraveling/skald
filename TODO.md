# Skald TODO

- [ ] Operations on normal beats
- [x] 2.4 Logic Beats
    - [x] One-liners
    - [x] Conditional logic beats
    - [x] Else logic beats
- [x] 3.1.3 Variables
    - [x] Declarations
    - [x] Variable carry-throughs
    - [x] Operator rvalue methods
    - [x] Operator rvalue variables
- [ ] 4.1.1 Module Transitions
    - [ ] 4.1.2 Module Entry Points
    - [ ] 4.2 Subfolder parsing
- [ ] 4.1.3 Conclusions
    - [ ] With arguments
- [ ] 5.2 Testbeds
    - [ ] Initial defaults
    - [ ] Including for carry-throughs

**Hold for later consideration**

We should hold off on these because it seems like their primary use will be PCG stuff, and it is decently likely there will be a better way to do that. If that decision is finalized we will need to update the syntax docs accordingly.

- [ ] 2.3.4 Chance Ternaries
    - [ ] Chance Switch Ternaries
    - [ ] Weights
    - [ ] Conditional options
- [ ] 2.5 Chance blocks
    - [ ] Weighting
    - [ ] With logic beats
    
**Then begin planning next steps:**

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
