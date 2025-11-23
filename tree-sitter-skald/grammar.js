module.exports = grammar({
  name: 'skald',

  extras: $ => [
    /[ \t]/,
    $.comment,
    $.empty,
  ],

  rules: {

    source_file: $ => seq(
      repeat($.variable_declaration),
      repeat($.testbed),
      repeat($.block),
    ),

    // Empty lines
    empty: $ => /\n/,

    // Comments
    comment: $ => token(seq('--', /.*/)),

    // Variable declarations
    variable_declaration: $ => seq(
      choice('~', '<'),
      field('name', $.variable_name),
      '=',
      field('value', $.rvalue),
      /\n/,
    ),

    variable_name: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    // Testbeds
    testbed: $ => seq(
      '@testbed',
      field('name', $.identifier),
      /\n/,
      repeat($.testbed_declaration),
      '@end',
      /\n/,
    ),

    testbed_declaration: $ => seq(
      field('variable', $.variable_name),
      '=',
      field('value', $.simple_value),
      /\n/,
    ),

    // Blocks
    block: $ => seq(
      field('tag', $.block_tag),
      repeat(choice(
        $.beat,
        $.logic_beat,
      )),
      optional($.choices),
    ),

    block_tag: $ => seq('#', $.identifier, /\n/),

    attribution: $ => token(seq(/[a-zA-Z_][a-zA-Z0-9_]*/, ':')),

    // Beats (dialogue/logic lines)
    beat: $ => prec.right(seq(
      optional($.conditional),
      optional(field('attribution', $.attribution)),
      field('content', $.text_content),
      /\n/,
      repeat($.indented_operation),
    )),

    logic_beat: $ => choice(
      // Multi-line: conditional/else, then operations on following lines
      seq(
        '*',
        choice($.conditional, '(else)'),
        /\n/,
        repeat1($.indented_operation),
      ),
      // Single-line: conditional/else with operation on same line
      seq(
        '*',
        choice($.conditional, '(else)'),
        $.operation,
        /\n/,
      ),
    ),

    // Text content with insertions
    text_content: $ => repeat1(choice(
      $.text_fragment,
      $.insertion,
      $.inline_comment,
    )),

    text_fragment: $ => token(prec(-1, /[^{}\n\r]+/)),

    inline_comment: $ => seq('{--', /[^}]*/, '}'),

    insertion: $ => seq(
      '{',
      choice(
        $.simple_insertion,
        $.ternary_insertion,
        $.switch_ternary,
      ),
      '}',
    ),

    simple_insertion: $ => $.rvalue,

    ternary_insertion: $ => seq(
      field('condition', $.rvalue),
      '?',
      field('true_value', $.rvalue),
      ':',
      field('false_value', $.rvalue),
    ),

    switch_ternary: $ => seq(
      field('switch_value', $.rvalue),
      '?',
      '[',
      commaSep1($.switch_case),
      ']',
    ),

    switch_case: $ => seq(
      field('key', $.simple_value),
      ':',
      field('value', $.rvalue),
    ),

    // Conditionals
    conditional: $ => seq(
      '(?',
      field('expression', $.conditional_expression),
      ')',
    ),

    conditional_expression: $ => choice(
      $.conditional_clause,
      seq(
        $.conditional_clause,
        repeat1(seq(
          choice('and', 'or'),
          $.conditional_clause,
        )),
      ),
    ),

    conditional_clause: $ => choice(
      seq('(', $.conditional_expression, ')'),
      $.conditional_atom,
    ),

    conditional_atom: $ => choice(
      seq('!', $.rvalue),
      $.rvalue,
      seq(
        $.rvalue,
        field('operator', choice('=', '!=', '>', '<', '>=', '<=')),
        $.rvalue,
      ),
    ),

    // Choices
    choices: $ => repeat1($.choice),

    choice: $ => seq(
      '>',
      optional($.conditional),
      field('text', $.text_content),
      optional(seq('->', field('target', $.identifier))),
      /\n/,
      repeat($.indented_operation),
    ),

    // Operations (must be indented when on separate lines)
    indented_operation: $ => prec.right(2, seq(
      /[ \t]+/,
      $.operation,
      /\n/,
    )),

    operation: $ => choice(
      $.move_op,
      $.go_module_op,
      $.exit_op,
      $.method_call_op,
      $.mutation_op,
    ),

    move_op: $ => seq('->', field('target', $.identifier)),

    go_module_op: $ => seq(
      'GO',
      field('module', $.module_path),
      optional(seq('->', field('tag', $.identifier))),
    ),

    module_path: $ => /[a-zA-Z0-9_\/]+\.ska/,

    exit_op: $ => seq('EXIT', optional($.rvalue)),

    method_call_op: $ => $.method_call,

    mutation_op: $ => seq(
      '~',
      field('variable', $.variable_name),
      field('operator', choice('=', '+=', '-=', '=!')),
      optional(field('value', $.rvalue)),
    ),

    // RValues (right-hand side values)
    rvalue: $ => choice(
      $.simple_value,
      $.variable_ref,
      $.method_call,
    ),

    simple_value: $ => choice(
      $.string,
      $.number,
      $.boolean,
    ),

    variable_ref: $ => $.variable_name,

    method_call: $ => seq(
      ':',
      field('method', $.identifier),
      '(',
      optional(commaSep($.rvalue)),
      ')',
    ),

    // Primitives
    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    string: $ => token(seq(
      '"',
      repeat(choice(
        /[^"\\]/,
        seq('\\', /./),
      )),
      '"',
    )),

    number: $ => /\d+(\.\d+)?/,

    boolean: $ => choice('true', 'false'),
  }
});

function commaSep1(rule) {
  return seq(rule, repeat(seq(',', rule)));
}

function commaSep(rule) {
  return optional(commaSep1(rule));
}
