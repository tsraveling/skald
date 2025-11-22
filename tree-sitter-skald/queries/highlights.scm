; Comments
(comment) @comment

; Block tags
(block_tag) @label

; Variable declarations
"~" @keyword
"<" @keyword
(variable_declaration
  name: (variable_name) @variable)

; Variable references
(variable_ref) @variable

; Testbeds
"@testbed" @keyword
"@end" @keyword
(testbed name: (identifier) @type)

; Attributions
(attribution) @attribute

; Conditionals
"(?" @punctuation.bracket
(conditional) @conditional
"and" @keyword.operator
"or" @keyword.operator
"!" @operator
"(else)" @keyword

; Logic beats
(logic_beat "*" @punctuation.special)

; Operations
"->" @operator
(move_op target: (identifier) @label)
"GO" @keyword
(go_module_op module: (module_path) @string.special.path)
(go_module_op tag: (identifier) @label)
"EXIT" @keyword

; Method calls
(method_call
  method: (identifier) @function)
":" @punctuation.delimiter

; Mutations
(mutation_op
  variable: (variable_name) @variable
  operator: _ @operator)

; Choices
">" @punctuation.special

; Insertions
"{" @punctuation.bracket
"}" @punctuation.bracket
"?" @operator
(ternary_insertion) @conditional
(switch_ternary) @conditional

; Inline comments
(inline_comment) @comment

; Values
(string) @string
(number) @number
(boolean) @boolean

; Operators
"=" @operator
"!=" @operator
">" @operator
"<" @operator
">=" @operator
"<=" @operator
"+=" @operator
"-=" @operator
"=!" @operator

; Punctuation
"(" @punctuation.bracket
")" @punctuation.bracket
"[" @punctuation.bracket
"]" @punctuation.bracket
"," @punctuation.delimiter
