# Skald TODO

- [ ] 2.3.4 Chance Ternaries
    - [ ] Chance Switch Ternaries
    - [ ] Weights
    - [ ] Conditional options
- [ ] 2.4 Logic Beats
    - [ ] One-liners
    - [ ] Conditional logic beats
    - [ ] Else logic beats
- [ ] 2.5 Chance blocks
    - [ ] Weighting
    - [ ] With logic beats
- [ ] 3.1.3 Variables
    - [ ] Declarations
    - [ ] Variable carry-throughs
    - [ ] Operator rvalue methods
    - [ ] Operator rvalue variables
- [ ] 4.1.1 Module Transitinos
    - [ ] 4.1.2 Module Entry Points
    - [ ] 4.2 Subfolder parsing
- [ ] 4.1.3 Conclusions
    - [ ] With arguments
- [ ] 5.2 Testbeds
    - [ ] Initial defaults
    - [ ] Including for carry-throughs
    
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
