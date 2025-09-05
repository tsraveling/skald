# Skald TODO

NEXT: for text queue, decide if we should stick in a beat or a choice. the text queue should be there when either is done, so just grab the queue and inject it as content into the appropriate entity then.
- And then also add choice block support.

- [x] Embedded comments: `some text [and this is embedded] here`
- [x] Attributed beats
- [x] Choices and choice sets
- [ ] Choice redirects (see below)

## Choice Redirects (2.2.1)

- [ ] First set up the operations queue and overall structure

An inline redirect is really just an inline operation; we should support any of these

- [x] So set up grammar for operations
- [x] Then have that as an optional one on the end of the choice
- [ ] Then figure out how to determine if it is present, and if it is present, add it as the only member of the choice's operations stack

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
