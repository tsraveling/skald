# The Skald Engine

## 1. Interface

### 1.1 The Event Thread.

Skald works by communicating back and forth on the **Event Thread**. The Event Thread carries Actions (xxx) taken by the user in the client into the engine, and Responses (xxx) back from there into the client. If the engine needs to make one or method call (Syntax 3.1.2), the Event Thread will instead pass a matching number of Queries (xxx). Queries must resolved with QueryAnswers (xxx) before the thread will continue to the actual Response.

This is useful because it makes method resolution (very) asynchronous. If you want to pause your script while the player plays a lengthy game of chess with the AI, you can simply do something like this:

```
#game_block

opponent: So you think you can beat me, eh?

* (? :play_chess()) -> win_block

opponent: Tough luck!

#win_block

opponent: Curses, foiled again!
```

Let's assume the player wins the match. The event thread will look like this:

1. **Response** containing that initial beat (this will be returned when injecting into the engine at `#game_block`
2. **Action** with index 0 (xxx) indicating to just continue
3. **Query** with the `play_chess` method name and no arguments
4. **QueryAnswer** with a boolean value ten minutes later after the chess minigame is resolved in the game code proper.
5. (With that resolution, the logic beat (Syntax 2.4) will resolve to true, and the player will be redirected to `win_block`.)
6. **Response** with the first beat in `#win_block`.

### 1.2 Actions

### 1.3 Responses

### 1.4 Queries

### 1.5 QueryAnswers

## 2. Starting and Ending Sessions

TBD

## 3. State

### 2.1 State Structure

### 2.2 The Skald "Bookmark"

### 2.3 Saving and Loading in bulk

### 2.4 Direct manipulation

## 4. Modules and Filestructure
