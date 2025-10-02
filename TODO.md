# Skald TODO

- [ ] Return to rvalue pop queue
- [ ] 

# Docs

## 3.2 Conditionals

**Conditionals** can determine if a choice will appear, if a beat will be included, or if a logic beat (XX) will be executed.

```
(? bool_var) This beat will only appear (and attached operations will only be run) if bool_var is true.

* (? bool_var)
  :call_method() -- Operations on this logic beat will only occur if
                 -- bool_var is true.

> (? bool_var) This choice will only be enabled if bool_var is true.
```

### 3.2.1 And / Or

Multiple conditions can be used with the `and` and `or` keywords:

```
(? bool_var and other_bool) This beat will only occur if both values are true

(? bool_var or other_bool) This beat will occur if either value is true
```

You can also nest parentheses:

```
(? bool_var and (a_bool or b_bool)) This text will occur if bool var is true, and either a_bool or b_bool is true.
```

### 3.2.2 Value Checks

You can check **equality** on any variable:

```
(? bool_var = true or str_var = "Test" or num_var = 3) This beat will show up if any of these variables match the given variable.
```

Likewise, you can check **not equals**:

```
(? bool_var != true or str_var != "Test" or num_var != 3) This beat will show up if any of these variables *don't* match the given variable.
```

Bools can be **false-checked** or **true-checked** directly:

```
(? !bool_var) This beat will show up if bool_var is false.
(? bool_var) This beat will show up if bool_var is true.
```

You can compare integers using **more and less than operators**:

```
(? num_var > 10) More than ten
(? num_var >= 10) More than or equal to ten
(? num_var < 10) Less than ten
(? num_var <= 10) Less than or equal to ten
```

### 3.2.3 Methods as Conditions

Any method can be used as a **boolean check**. If the method does not return a response, that will be treated as a **false** value.

```
(? :check_method()) -- This will fire if check_method() returns true via the API.
```

Likewise the false boolean check syntax applies to methods:

```
(? !:check_method()) -- This will fire if check_method() returns false via the API.
```

You can also pass arguments to conditional methods:

```
(? :check_method(some_variable)) -- This will pass `some_variable` to `check_method`, where it can be used in determining the calculation
```

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
