# Skald TODO

A: process choice

- [ ] Consolidate consumption of members in grammar and actions
- [ ] Ensure that these entities get their correct members after grammar etc updates
- [ ] Ensure conditionals are being properly handled
- [ ] When composing members, throw a warning if anything comes after a transition

## Issues

- [ ] Currently ChoiceGroups are not running at all
- [ ] Mutations / Notifications etc are not being handled by Skalder
- [ ] Mutations are also not processing at all now

## Methods

- [ ] Method should still await return (ie function as query) when called, and should write out to log.

## Codex

- [ ] Separate grammar, action, and parse state file
- [ ] But codex entities still in Skald.h
- [ ] Refactor Engine to start by loading a codex
- [ ] Then load module
- [ ] Refactor Skalder to automatically check for the nearest parent codex when running a module, and output that to log
