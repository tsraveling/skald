# Skald TODO

## 2.3 Insertions

### 2.3.1 Direct Insertion

You can insert the value of a variable into any text (choice or beat) using **curly braces**, like this:

```
This is some beat text! Our variable "player_name" is currently set to {player_name}.
```

### 2.3.2 Boolean Ternaries

You can do a **boolean ternary insertion** like this:

```
Right now it is {is_day ? "light" : "dark"}
```

The left-hand side of the ternary is just a basic conditional (3.2), and can use `and` / `or`, parentheses, and so on. The right hand side can either be a string, or a variable that resolves to a string, like this:

```
The value of either a variable or a method is {some_condition ? a_variable : "some text"}
```

### 2.3.3 Switch Ternaries

You can also do a **switch ternary** using an integer variable like this:

```
My pet is a {pet_type ? 1:dog 2:cat 3:donkey}.
```

### 2.3.4 Chance Ternaries

You can use a **chance ternary** to inject an option at random:

```
I prefer {% ? "dark" : "light"} roast coffee.
```

You can also make a **chance switch ternary**:

```
Her pet is a {% ? :"dog" :"cat" :"donkey"} -- these choices are evenly weighted
```

You can weight options like this:

```
Her pet is a {% ? :"dog" 3:"cat" 5:"donkey"} -- This pet has a 5/9 chance of being a donkey
```

Finally, you can mark some options as conditional with paren syntax:

```
Her pet is a {% ? :"dog" :"cat" 3(?is_pirate): "parrot"}.
```

In this example, if `is_pirate` is not set, it's a coin-flip between dog and cat. If she is a pirate, however, the pet has a 3/5 chance of being a parrot.


## 2.4 Logic Beats

Sometimes in a narrative flow, you may want to perform logic that is not directly attached to a text beat. To do that, you can use a **logic beat:**

```
*                 -- this is a logic beat
  :call_method
  ~ some_var = 3
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

# Edge Cases

- [ ] Methods as arguments of other methods
