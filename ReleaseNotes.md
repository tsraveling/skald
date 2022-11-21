# 0.1 - 0.3

Prior iteration; more geared toward procedural text, without any kind of choice logic. 

# 0.4

Initial working release.

## 0.4.1

- Added logic blocks
- Added single transition rule

# 0.5

- Added in-tool test engine for testing scripts while developing (`skald test ./some-file.ska`).
- Added `@testbed` feature for defining values for test purposes.

# 0.6

## 0.6.1

- Enabled snake_case for input names
- Transitioned test engine to use block return architecture and added "continue" for pacing

## 0.6.2

- Fixed some bugs with the test engine's playback

## 0.6.3

- Replaced simple signal array with an array of objects:
  ```
  [{
    signal: (string)
    value: (optional value, of input type -- int, string, or bool)
  }, ...]
  ```

## 0.6.4

- Clarified error messaging for improperly formatted conditional lines

## 0.6.5

- Added input injection