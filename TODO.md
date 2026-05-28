# Skald TODO

A: process choice

- [ ] Consolidate consumption of members in grammar and actions
- [ ] Ensure that these entities get their correct members after grammar etc updates
- [ ] Ensure conditionals are being properly handled
- [ ] When composing members, throw a warning if anything comes after a transition

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
