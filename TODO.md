# Skald TODO

NEXT: for text queue, decide if we should stick in a beat or a choice. the text queue should be there when either is done, so just grab the queue and inject it as content into the appropriate entity then.
- And then also add choice block support.

- [ ] Embedded comments: `some text [and this is embedded] here`
- [ ] Attributed beats
- [ ] Choices and choice sets
- [ ] Choice redirects

- [ ] This covers up to 2.2.1 in the syntax doc. Plan the rest of it in 003.

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
