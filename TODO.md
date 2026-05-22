# Skald TODO

A: process choice

- [x] advance_cursor now needs to walk through the open conditional block as well

- Transitions should still be silent.
- Exit and Go already have Responses
- [ ] Methods need to happen inline
- [ ] Mutation needs -> Notification

### NEXT:

- [ ] Make var_set etc return notification
- [ ] Chase that up the tree

## Methods

- [ ] Method should still await return (ie function as query) when called, and should write out to log.

## Codex

- [ ] Separate grammar, action, and parse state file
- [ ] But codex entities still in Skald.h
- [ ] Refactor Engine to start by loading a codex
- [ ] Then load module
- [ ] Refactor Skalder to automatically check for the nearest parent codex when running a module, and output that to log
