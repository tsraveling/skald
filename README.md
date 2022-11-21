# SKALD
A procedural narrative scripting language and toolset used for interactive fiction projects.

# Installation

Just run this to install:

`npm i -g skald-compile`

Note: On Windows, you will have to enable script execution:
- Run PowerShell as administrator
- `Set-ExecutionPolicy -ExecutionPolicy RemoteSigned`. This will notify powershell that external
  npm scripts are signed for, and can be trusted.

# Command Line Tool

Run `skald {source directory} {destination directory}` to run the compiler. JSON files will be
written into the destination directory, overwriting anything in it already. Errors
will be printed to the console if found. The source directory will not be modified in any way.

# Language

## Comments

**Comments** are preceded by `//`, either on their own line or on the end of a line:

```
// This is a comment
bill: Shut up, Ted // This is a comment as well
```

## Sections

Skald navigates via **sections**, which are marked like this:

```
#some-section
```

When a section is entered, processing progresses sequentially through the blocks. If it reaches
a transition, that block is processed, and then navigation takes the player to the target section
without processing the remaining blocks in that section.

If no transition is found, and there are choices, the choices are displayed to the player (see below).

If no choices are found either, the player automatically transitions to the next section.

The software will always start with the first section in the file. You can use a **logic block** (see below) if you
would like to create branching logic or a simple redirect from there. By convention, the first section is usually
called `#start`, but you can use whatever name you like.

## Blocks

A **block** is can be either **text** or just **logic**. A **text** block is marked by attribution, like
this:

```
bill: Be excellent to one another!
```

The both the attribution tag (`bill`) and the body (`Be excellent to one another!`) will be passed up into
the game code. The tag can be used to identify characters, or for whatever you wish, but must be alphanumeric.

A logic block is marked with a single asterisk:

```
*
    -> isolated-transition
```

This is generally combined with a **condition** or other **meta** (see below) to create branching or conditional
logic within sections.

## Choices

Choices *must* follow blocks within a section. Choices are displayed to the user at the end of a section,
assuming meta conditions are met (see below). If the player selects a given choice, its meta
transitions, signals, and mutations are activated. If no transition is supplied, the player remains on
that group of choices.

Choices are marked with a `>`, like this:

```
> Go west
```

## Meta

Both choices and blocks can have **meta**. There are four types of meta: conditions, mutations, signals,
and transitions. Every meta is on its own line.

**Conditions** determine whether a block is processed, and whether a choice is presented to the player.
Other meta is processed when a choice is selected, or when a block is reached (if that block's conditions
are met).

```
*
    ? someValue < 10
    ~ someOtherValue = 100
    :callMethod
    -> another-section
```

### Conditions

Conditions are preceded with `?`. There are two kinds: boolean, and operator-based. Boolean conditions
check a simple boolean flag, and can look like either `? checkBoolean` or `? !checkBoolean`. Everything
else uses an operator, like this: `? someValue < 10`. The input (`someValue` in this case), operator (`<`)
and value (`10`) are sent to the host game whole -- it is up to you to define how they are interpreted.

Valid operators for conditions are `>=`, `<=`, `>`, `<`, `!=`, and `==`.

**Text blocks** will not be displayed if a condition is not met. **Logic blocks** will not be processed
if not met. And **choices** will be left out of the list of choices presented to the user.

### Mutations

Mutations notify the host software to update an **input** (aka, a variable registered on the software layer)
in some way. For instance:

```
*
    ~ totalDistance += 10
```

Tells the software to process the input `totalDistance` with the operator `+=` and the value `10`. How those
operators work is up to the software.

Syntactically valid operators for mutations include the characters `+`, `-`, and `=`, in any configuration, however,
the operators that work in the CLI testbed and the Godot plugin are:

- `=!` flips a boolean flag
- `=` sets the value of an input
- `+=` and `-=` increment and decrement an input's value

**Note on boolean values:** by convention, doing this with a boolean flag: `~ someFlag = !` will reverse the
value of that flag.

## Signals

Signals are simply called on their own line, like this:

```
*
    :someSignal
```

This will send a `signal` message to the software with the tag `someSignal`. It can be used to trigger methods or
other logic in code.

## Exits

If a meta hits the single word `END`, this will signal the software to leave the conversation here. The other meta
in the block or choice (not including transitions ) will be processed first.

## Input Injection

You can inject the value of an input into block or choice text using the simple syntax `{input_name}`.

# Testing

You can test any script by using `skald test {filename}`. This will compile the Skald file on
the fly, and will inform you of any compiler errors. From within the test interface, you can
type `help` for an extended list of commands.

## Testbeds

You can define **testbeds** right in your Skald file to make testing easier. A testbed looks
like this:

```
@testbed example-bed
    someValue = 10
    someBoolean = true
    someString = happy
@end
```

A testbed **must** be opened with `@testbed {tag}` and closed with `@end`. Indentation is up to
the implementer. Within the testbeds, use simple `{input} = {value}` syntax. When using the
testing tool, the first testbed will be loaded by default. You can then restart with a
different testbed by using the `restart {testbed}` command. You can also list testbeds
with the `testbeds` command.

This can be used to mock various potential game states.