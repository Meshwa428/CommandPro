# Synapse v2 — Formal Grammar

Status: **draft** · Parser: recursive descent + Pratt for expressions

## Notation (EBNF)

```
::=          definition
|            alternative
( )          grouping
[ X ]        optional  (zero or one)
{ X }        repetition (zero or more)
{ X }+       one or more
'x'          terminal string
UPPER        terminal token (defined in §1)
/* */        comment
```

---

## 1. Lexical Grammar (Tokens)

The lexer runs before the parser. It produces a flat stream of tokens.
Whitespace (spaces, tabs) is skipped between tokens except inside strings.
**Newlines are significant** — they act as statement terminators (see §2.1).

### 1.1 Character Classes

```ebnf
LETTER     ::= 'a'..'z' | 'A'..'Z'
DIGIT      ::= '0'..'9'
HEX_DIGIT  ::= DIGIT | 'a'..'f' | 'A'..'F'
ALPHA_NUM  ::= LETTER | DIGIT | '_'
```

### 1.2 Whitespace and Comments

```ebnf
SKIP       ::= ' ' | '\t' | '\r'               /* silently discarded */
COMMENT    ::= '#' { any char except '\n' }     /* to end of line, discarded */
NEWLINE    ::= '\n'                             /* significant — see §2.1 */
```

### 1.3 Integer Literals

```ebnf
INT_LIT    ::= DEC_INT | HEX_INT | BIN_INT | OCT_INT
DEC_INT    ::= DIGIT { DIGIT | '_' }            /* 1_000_000 allowed */
HEX_INT    ::= '0x' HEX_DIGIT { HEX_DIGIT | '_' }
BIN_INT    ::= '0b' ('0'|'1') { '0'|'1'|'_' }
OCT_INT    ::= '0o' ('0'..'7') { '0'..'7'|'_' }
```

### 1.4 Float Literals

```ebnf
FLOAT_LIT  ::= DIGIT { DIGIT } '.' { DIGIT } [ EXPONENT ]
             | DIGIT { DIGIT } EXPONENT
EXPONENT   ::= ('e' | 'E') [ '+' | '-' ] DIGIT { DIGIT }
```

### 1.5 Duration Literals (first-class type)

```ebnf
DURATION   ::= (INT_LIT | FLOAT_LIT) DUR_UNIT
DUR_UNIT   ::= 'ms' | 's' | 'm' | 'h'
/* Examples: 500ms  2s  1.5m  0.5h */
/* Lexer greedily matches: '500ms' is one DURATION token, not INT+'ms' */
```

### 1.6 String Literals

```ebnf
STRING_LIT    ::= RAW_STRING | INTERP_STRING | MULTILINE_STRING

RAW_STRING    ::= 'r"' { RAW_CHAR } '"'
RAW_CHAR      ::= any char except '"'         /* no escape processing */

INTERP_STRING ::= '"' { STR_FRAGMENT } '"'
STR_FRAGMENT  ::= STR_CHAR | ESCAPE_SEQ | INTERP_BLOCK | '{{' | '}}'
STR_CHAR      ::= any char except '"' '\' '{' '}'
INTERP_BLOCK  ::= '{' /* yields INTERP_OPEN; then normal token stream; '}' yields INTERP_CLOSE */
ESCAPE_SEQ    ::= '\n' | '\t' | '\\' | '\"' | '\r' | '\0' | '\x' HEX_DIGIT HEX_DIGIT

MULTILINE_STRING ::= '"""' { any char } '"""'
/* Leading newline after opening """ is stripped. Indentation stripped to min indent. */
```

> **Lexer implementation note:** String interpolation is handled by a
> sub-lexer mode. On `{`, the lexer pushes `MODE_INTERP` and produces
> `INTERP_OPEN`. It then lexes normal tokens until the matching `}`,
> which produces `INTERP_CLOSE`. Braces nest correctly inside interpolations.

### 1.7 Identifiers and Keywords

```ebnf
IDENT ::= (LETTER | '_') { ALPHA_NUM }
          /* EXCEPT keywords listed below */
```

**Language keywords** — may never be used as identifiers:

```
let    const    fn      return   if      else     while
repeat for      in      break    continue match    case
try    catch    finally throw    and      or       not
is     use      as      true     false   none
```

**Type-name keywords** — reserved in pattern context, usable as identifiers elsewhere:

```
int    float    string    bool    list    map    tuple
```

**Command keywords** — reserved at statement start, usable as identifiers in
expression context. The parser uses position to disambiguate.

```
mouse   click   drag    scroll   hold    release  press   type
run     open    close   focus    move    resize   maximize  minimize
capture wait    find    see      tap     check    uncheck   select
read
```

### 1.8 Operators and Punctuation

```
+   -   *   /   //   %   **      /* arithmetic */
==  !=  <   <=  >   >=          /* comparison */
=   +=  -=  *=  /=  //=  %=  **=  ??=  /* assignment */
|>                              /* pipe */
??                              /* null coalescing */
?.                              /* optional chain — TWO-char token */
->                              /* reserved, unused in v2 */
(  )  [  ]  {  }
,  .  :  ;
```

---

## 2. Syntactic Grammar

### 2.1 Statement Termination

A statement ends at a **NEWLINE** or at **';'** (inline separator).
The following NEWLINES are **continuation** (not terminators):

- After an opening `{`, `[`, `(`
- After a binary operator: `+`, `-`, `*`, `/`, `//`, `%`, `**`, `==`, `!=`,
  `<`, `<=`, `>`, `>=`, `and`, `or`, `not`, `|>`, `??`
- After `,`
- After `else`, `catch`, `finally`

In all other positions, a NEWLINE terminates the statement.
This matches Python-style implicit line continuation inside brackets.

```ebnf
TERM ::= NEWLINE | ';'
```

### 2.2 Program

```ebnf
program    ::= stmt_list EOF
stmt_list  ::= { stmt TERM }
```

---

### 2.3 Statements

```ebnf
stmt ::=
    let_stmt
  | const_stmt
  | assign_stmt
  | if_stmt
  | while_stmt
  | repeat_stmt
  | for_stmt
  | match_stmt
  | try_stmt
  | fn_decl
  | return_stmt
  | throw_stmt
  | break_stmt
  | continue_stmt
  | use_stmt
  | in_scope_stmt
  | cmd_stmt        /* automation command — starts with command keyword */
  | expr_stmt       /* expression used as statement (calls, assignments) */
```

#### 2.3.1 Variable Declaration

```ebnf
let_stmt   ::= 'let' bind_list '=' expr_list
const_stmt ::= 'const' IDENT '=' expr

bind_list  ::= IDENT { ',' IDENT }
expr_list  ::= expr { ',' expr }

/* Examples:
     let x = 5
     let a, b = 1, 2
     let (x, y) = point          -- destructuring: see §2.3.2
     const PI = 3.14159
*/
```

#### 2.3.2 Destructuring Assignment

Handled as a special form of `let_stmt` when the left side is a
parenthesised or bracketed bind list:

```ebnf
destruct_let ::=
    'let' '(' IDENT { ',' IDENT } ')' '=' expr        /* tuple destruct */
  | 'let' '[' IDENT { ',' IDENT } ']' '=' expr        /* list destruct */
  | 'let' '{' destruct_map_entry { ',' destruct_map_entry } '}' '=' expr  /* map destruct */

destruct_map_entry ::= IDENT [ ':' IDENT ]    /* {name} or {name: alias} */

/* Examples:
     let (x, y) = (300, 400)
     let (first, *rest) = items
     let {name, age} = user
     let {name: username} = user
*/
```

`*IDENT` inside a destruct bind list captures the remaining elements.

#### 2.3.3 Assignment and Augmented Assignment

```ebnf
assign_stmt     ::= lvalue_list '=' expr_list
aug_assign_stmt ::= lvalue aug_op expr

lvalue_list ::= lvalue { ',' lvalue }
lvalue      ::= IDENT
              | postfix_expr '[' expr ']'          /* index */
              | postfix_expr '.' IDENT             /* field */

aug_op ::= '+=' | '-=' | '*=' | '/=' | '//=' | '%=' | '**=' | '??='

/* Disambiguation: assign_stmt starts only when the parser sees
   IDENT/lvalue followed by '=' that is NOT part of '==', '<=', '>=', '!='.
   An lvalue ',' signals multi-assignment.
   Swap:  a, b = b, a   (expr_list on right, lvalue_list on left) */
```

#### 2.3.4 If Statement

```ebnf
if_stmt ::= 'if' expr block
            { 'else' 'if' expr block }
            [ 'else' block ]

/* No parentheses around condition.
   No 'elif' — always 'else if' (two tokens). */
```

#### 2.3.5 While Statement

```ebnf
while_stmt ::= 'while' expr block
```

#### 2.3.6 Repeat Statement

```ebnf
repeat_stmt ::= 'repeat' expr [ 'as' IDENT ] block

/* repeat 5 { }            -- no index
   repeat 5 as i { }       -- 0-based index i */
```

#### 2.3.7 For Statement

```ebnf
for_stmt ::=
    'for' iter_bind 'in' iter_source block

iter_bind ::=
    IDENT                        /* for x in ... */
  | IDENT ',' IDENT             /* for k, v in map / for i, x in enumerate() */

iter_source ::=
    range_src                   /* 0 to 9  /  0 to 10 by 2  /  9 to 0 by -1 */
  | expr                        /* any iterable */

range_src ::= expr 'to' expr [ 'by' expr ]

/* 'to' is ONLY a range operator inside iter_source.
   Everywhere else, 'to' is not a valid operator.
   The parser enforces this contextually. */
```

#### 2.3.8 Match Statement

```ebnf
match_stmt ::= 'match' expr '{' NEWLINE
               { match_arm }
               [ else_arm ]
               '}'

match_arm  ::= 'case' pattern [ 'if' expr ] block
else_arm   ::= 'else' block
```

#### 2.3.9 Patterns (used in match)

```ebnf
pattern ::=
    literal_pattern           /* case 42  /  case "ok"  /  case true */
  | type_pattern              /* case int  /  case string  /  case list */
  | tuple_pattern             /* case (0, 0)  /  case (x, y) */
  | capture_pattern           /* case x  — binds match value to x */
  | wildcard_pattern          /* case _  — discards */

literal_pattern ::= INT_LIT | FLOAT_LIT | STRING_LIT | BOOL_LIT | NONE_LIT

type_pattern    ::= 'int' | 'float' | 'string' | 'bool'
                  | 'list' | 'map' | 'tuple' | 'none'

tuple_pattern   ::= '(' pattern { ',' pattern } ')'

capture_pattern ::= IDENT   /* any name that isn't a literal or type keyword */

wildcard_pattern ::= '_'

/* Disambiguation rule:
   'case int'    → type_pattern (int is a type keyword)
   'case x'      → capture_pattern (x is a fresh binding)
   'case 42'     → literal_pattern
   'case (x, y)' → tuple_pattern (nested captures) */
```

#### 2.3.10 Try Statement

```ebnf
try_stmt ::= 'try' block
             { catch_clause }
             [ else_clause ]
             [ finally_clause ]

catch_clause   ::= 'catch' IDENT [ 'if' expr ] block
else_clause    ::= 'else' block        /* runs ONLY if no exception was thrown */
finally_clause ::= 'finally' block     /* always runs */

/* At least one of: catch, else, finally must be present.
   Multiple catch clauses allowed for typed catching:
     catch err if err.type == "io" { }
     catch err { }                       -- default catch
   Order: catches checked top to bottom. */
```

#### 2.3.11 Function Declaration

```ebnf
fn_decl   ::= 'fn' IDENT '(' [ param_list ] ')' block

param_list ::= param { ',' param }
param      ::= IDENT [ '=' expr ]    /* positional, optional default */
             | '*' IDENT             /* variadic rest — must be last */

/* Examples:
     fn add(x, y) { return x + y }
     fn greet(who = "world") { say "hello {who}" }
     fn connect(host, port = 8080, timeout = 30s) { }
     fn sum(*nums) { }
*/
```

#### 2.3.12 Return / Throw / Break / Continue

```ebnf
return_stmt   ::= 'return' [ expr { ',' expr } ]   /* multiple → tuple */
throw_stmt    ::= 'throw' expr
break_stmt    ::= 'break'
continue_stmt ::= 'continue'
```

#### 2.3.13 Use (Module Import)

```ebnf
use_stmt ::=
    'use' module_ref [ 'as' IDENT ]                  /* import whole module */
  | 'use' module_ref '{' IDENT { ',' IDENT } '}'     /* selective import */
  | 'use' module_ref ',' module_ref { ',' module_ref }  /* multi-use */

module_ref ::= STRING_LIT | IDENT { '.' IDENT }

/* Examples:
     use math
     use mouse, keyboard
     use "helpers.syn" as helpers
     use math { sqrt, abs }
     use spotify_flows
*/
```

#### 2.3.14 In-Scope Block

```ebnf
in_scope_stmt ::= 'in' STRING_LIT block

/* Scopes all find/see/tap/find commands to the named window.
   Example: in "Firefox" { tap input "Search" } */
```

#### 2.3.15 Expression Statement

```ebnf
expr_stmt ::= expr

/* An expression used as a statement.
   Common cases: function calls, pipe chains.
   The result value is discarded. */
```

---

### 2.4 Automation Command Statements

Command statements desugar to stdlib calls at compile time.
They are parsed as distinct statement forms (not expression statements)
when the leading token is a command keyword at statement position.

```ebnf
cmd_stmt ::=
    mouse_cmd
  | click_cmd
  | drag_cmd
  | scroll_cmd
  | hold_cmd
  | release_cmd
  | press_cmd
  | type_cmd
  | run_cmd
  | open_cmd
  | close_cmd
  | focus_cmd
  | move_cmd
  | resize_cmd
  | maximize_cmd
  | minimize_cmd
  | capture_cmd
  | wait_cmd
  | tap_cmd
  | find_cmd
  | see_cmd
  | check_cmd
  | uncheck_cmd
  | select_cmd
  | read_cmd
```

#### Coordinate pair

```ebnf
coord ::= expr ',' expr    /* x, y */
```

#### Mouse commands

```ebnf
mouse_cmd   ::= 'mouse' coord [ ',' expr ]
/* mouse 300, 400           -- natural speed (RAT model)
   mouse 300, 400, 0.5      -- speed multiplier */

click_cmd   ::= 'click' [ btn ] [ coord ] [ ',' INT_LIT ]
/* click                    -- left click at current position
   click right 100, 200     -- right click at coord
   click 100, 200, 3        -- triple click (n >= 1, enforced)
   click left 100, 200, 2   -- btn + coord + count */

drag_cmd    ::= 'drag' coord ',' coord [ ',' expr ]
/* drag 10, 10, 400, 300
   drag 10, 10, 400, 300, 0.7  -- with speed */

scroll_cmd  ::= 'scroll' scroll_dir expr
/* scroll up 3   scroll down 5   scroll left 2   scroll right 2 */

hold_cmd    ::= 'hold'    ( btn | key_chord )
release_cmd ::= 'release' ( btn | key_chord )

btn         ::= 'left' | 'right' | 'middle'
scroll_dir  ::= 'up'    | 'down' | 'left'   | 'right'
```

#### Keyboard commands

```ebnf
press_cmd ::= 'press' key_chord
type_cmd  ::= 'type' expr

key_chord ::= key_name { '+' key_name }
key_name  ::= IDENT    /* enter, ctrl, alt, shift, win, tab, esc,
                          f1..f12, backspace, delete, home, end,
                          up, down, left, right, pageup, pagedown,
                          space, caps, insert, a..z, 0..9 */
/* Examples: enter   ctrl+c   alt+f4   win+s   ctrl+shift+esc */
```

#### App and window commands

```ebnf
run_cmd      ::= 'run'      expr
open_cmd     ::= 'open'     expr
close_cmd    ::= 'close'    expr
focus_cmd    ::= 'focus'    expr
move_cmd     ::= 'move'     STRING_LIT coord    /* STRING_LIT = window name */
resize_cmd   ::= 'resize'   STRING_LIT coord
maximize_cmd ::= 'maximize' STRING_LIT
minimize_cmd ::= 'minimize' STRING_LIT
```

#### Screen and system

```ebnf
capture_cmd ::= 'capture' [ coord ',' coord ] STRING_LIT
/* capture "shot.png"
   capture 0, 0, 1920, 1080 "region.png" */

wait_cmd ::= 'wait' expr   /* expr must evaluate to Duration or numeric (seconds) */
```

#### UI element commands

```ebnf
elem_type ::= 'button' | 'input' | 'checkbox' | 'radio' | 'dropdown'
            | 'link'   | 'icon'  | 'toggle'   | 'slider'| 'tab' | 'menu_item'

tap_cmd    ::= 'tap'  [ elem_type ] expr [ confidence_clause ]
find_cmd   ::= 'find' [ elem_type ] expr
see_cmd    ::= 'see'  [ elem_type ] expr [ confidence_clause ]
check_cmd  ::= 'check'   expr
uncheck_cmd::= 'uncheck' expr
select_cmd ::= 'select' expr 'in' expr
read_cmd   ::= 'read' expr      /* returns string value of element */

confidence_clause ::= 'min_confidence' FLOAT_LIT
```

---

### 2.5 Expressions

Expressions are parsed by a Pratt parser (top-down operator precedence).
Precedence levels listed **low → high** (lower number = lower precedence = evaluated last).

```
Level  Operator(s)                    Associativity
─────  ────────────────────────────   ─────────────
  1    ternary: X if C else Y         none (special)
  2    |>                             left
  3    ??                             left
  4    or                             left
  5    and                            left
  6    not                            prefix (right)
  7    == != < <= > >= in not in      left (chainable)
       is  is not
  8    + -  (addition)                left
  9    * / // %                       left
 10    unary -  (negation)            prefix (right)
 11    **                             right
 12    () [] . ?.  (postfix)          left
```

#### 2.5.1 Grammar Productions

```ebnf
expr ::= ternary_expr

ternary_expr ::=
    pipe_expr 'if' pipe_expr 'else' pipe_expr    /* X if C else Y */
  | pipe_expr

pipe_expr ::= null_expr { '|>' null_expr }       /* left-associative */

null_expr ::= or_expr { '??' or_expr }           /* left-associative */

or_expr ::= and_expr { 'or' and_expr }

and_expr ::= not_expr { 'and' not_expr }

not_expr ::= 'not' not_expr | cmp_expr

cmp_expr ::=
    add_expr { cmp_op add_expr }                 /* 0 < x < 100 chains */
  | add_expr ( 'is' | 'is' 'not' ) type_name
  | add_expr ( 'in' | 'not' 'in' ) add_expr

cmp_op ::= '==' | '!=' | '<' | '<=' | '>' | '>='

add_expr ::= mul_expr { ('+' | '-') mul_expr }

mul_expr ::= unary_expr { ('*' | '/' | '//' | '%') unary_expr }

unary_expr ::= '-' unary_expr | pow_expr         /* unary minus */

pow_expr ::= postfix_expr [ '**' unary_expr ]    /* right-associative */

postfix_expr ::= primary_expr { postfix_op }

postfix_op ::=
    '(' [ arg_list ] ')'               /* call:   f(x, y) */
  | '[' expr ']'                       /* index:  xs[0] */
  | '[' [ expr ] 'to' [ expr ] ']'    /* slice:  xs[1 to 3] */
  | '.' IDENT                          /* field:  obj.name */
  | '?.' IDENT                         /* opt chain: obj?.name */

arg_list ::= arg { ',' arg }
arg      ::= IDENT ':' expr            /* named:  port: 3000 */
           | expr                      /* positional */
/* Named args may appear in any order after positional args. */

primary_expr ::=
    INT_LIT
  | FLOAT_LIT
  | BOOL_LIT                           /* true | false */
  | NONE_LIT                           /* none */
  | DURATION                           /* 2s  500ms  1m  0.5h */
  | STRING_LIT                         /* "hello {name}" */
  | IDENT                              /* variable reference */
  | '(' expr ')'                       /* grouped */
  | tuple_expr
  | list_expr
  | map_expr
  | set_expr
  | fn_expr                            /* anonymous function */
  | match_expr                         /* match as value */
```

#### 2.5.2 Tuple Expression

```ebnf
tuple_expr ::= '(' expr ',' { expr ',' } [ expr ] ')'

/* A tuple requires at least ONE trailing comma after the first element,
   or at least two elements separated by comma:
     (1, 2)      → tuple of two ints
     (1,)        → tuple of one int
     (1)         → grouped expression (integer 1), NOT a tuple */
```

#### 2.5.3 List Expression

```ebnf
list_expr ::=
    '[' ']'                                                      /* empty list */
  | '[' expr { ',' expr } [ ',' ] ']'                           /* literal */
  | '[' expr comp_for { comp_for } [ 'if' expr ] ']'            /* comprehension */

comp_for ::= 'for' iter_bind 'in' expr
```

#### 2.5.4 Map Expression

```ebnf
map_expr ::=
    '{' '}'                                                      /* empty map */
  | '{' map_pair { ',' map_pair } [ ',' ] '}'                   /* literal */
  | '{' expr ':' expr comp_for [ 'if' expr ] '}'                /* comprehension */

map_pair ::= map_key ':' expr
map_key  ::= IDENT | STRING_LIT | INT_LIT

/* Bare IDENT keys are treated as string keys:
     {name: "mesh"}   ≡   {"name": "mesh"} */
```

#### 2.5.5 Set Expression

```ebnf
set_expr ::= 'set' '(' '[' expr { ',' expr } ']' ')'
/* No literal syntax — constructor form only to avoid {…} ambiguity with maps */
```

#### 2.5.6 Anonymous Function Expression

```ebnf
fn_expr ::= 'fn' '(' [ param_list ] ')' block

/* param_list same as fn_decl (§2.3.11) — defaults and *rest allowed */
/* Examples:
     fn(x) { return x * 2 }
     fn(x, y) { return x + y }
     fn(n = 0) { return n + 1 }
*/
```

#### 2.5.7 Match Expression (as value)

```ebnf
match_expr ::= 'match' expr '{' NEWLINE
               { match_arm }
               [ else_arm ]
               '}'

/* Each arm implicitly returns its last expression.
   No 'return' needed inside match arms.
   Used in: let label = match status { case 200 { "ok" } else { "err" } } */
```

---

### 2.6 Block

```ebnf
block ::= '{' NEWLINE stmt_list '}'
        | '{' stmt '}'             /* single-statement shorthand (same line) */
```

---

### 2.7 Disambiguation Rules

#### 2.7.1 Command keyword vs identifier

`mouse`, `press`, `type`, `run`, etc. are **command keywords**. They are only
parsed as commands at **statement position** (the start of a stmt). In
expression position they are treated as identifiers.

```syn
mouse 300, 400              # statement → mouse_cmd
let m = mouse               # expression → IDENT "mouse"
```

The parser's `stmt()` function checks the lookahead token:
if it is a command keyword, it dispatches to the appropriate command parser.
Otherwise it falls through to `expr_stmt`.

#### 2.7.2 `if` as ternary vs statement

`if` at statement position → `if_stmt`.
`if` after a complete expression (inside another expression) → ternary suffix.

```syn
if x > 0 { say "pos" }                     # if_stmt
let label = "big" if score > 90 else "small"  # ternary
```

The Pratt parser for `expr` handles ternary as a low-precedence infix/suffix rule.

#### 2.7.3 `match` as statement vs expression

`match` is parsed the same way in both contexts.
The statement form is just `match_expr` used as `expr_stmt`.

#### 2.7.4 `(expr)` vs tuple

A parenthesised expression is NOT a tuple unless it contains a comma:
```
(1)      → int 1    (grouped)
(1,)     → (1,)     tuple of one element
(1, 2)   → (1, 2)   tuple of two elements
```

#### 2.7.5 Chained comparisons

```syn
if 0 < x < 100 { ... }
```

Desugars to: `(0 < x) and (x < 100)`, with `x` evaluated once.
The compiler generates: `LT tmp, 0, x` + `JF tmp, end` + `LT tmp, x, 100`.

#### 2.7.6 `else if` vs `elif`

Only `else if` is valid (two tokens). `elif` is a syntax error.
The parser, after seeing `else`, checks if the next token is `if`.
If yes → else-if chain. If no → final else block.

#### 2.7.7 Map `{` vs block `{`

A `{` is a **map literal** only in expression position.
A `{` after `if`/`while`/`for`/`fn`/etc. is always a **block**.

#### 2.7.8 `is` vs `is not`

`is not` is a two-token operator, NOT `is` followed by expression `not`.
The parser consumes both tokens when it sees `is` followed by `not`.

#### 2.7.9 Optional chaining `?.`

`?.` is a single two-character token. The lexer produces `OPTCHAIN` for `?.`
and `QMARK` for standalone `?` (currently unused in v2, reserved).

#### 2.7.10 String `{` inside interpolation

Inside `"..."`, the lexer switches to interpolation mode on `{`.
Braces nest: `"{f({x})}"` has the outer `{` and `}` for interpolation,
and the inner `{x}` is part of the function call argument.

#### 2.7.11 `to` in range vs elsewhere

`to` is only a range operator inside `iter_source` (for-loop) and
inside `arg` position (e.g., `list(0 to 9)`). The parser enables range
parsing contextually. In all other expression positions, `to` is treated
as an identifier (not reserved).

---

### 2.8 Operator Precedence Table (Summary)

```
Prec  Operator                  Assoc    Notes
────  ────────────────────────  ──────   ─────────────────────────────
  1   X if C else Y             none     ternary suffix
  2   |>                        left     pipe
  3   ??                        left     null coalescing
  4   or                        left
  5   and                       left
  6   not X                     right    prefix
  7   == != < <= > >=           left     chainable (0 < x < 100)
      in  not in                left
      is  is not                left
  8   + -                       left
  9   * / // %                  left
 10   -X                        right    unary minus
 11   **                        right
 12   X()  X[]  X[a to b]  X.f  left     postfix / call / index / field
      X?.f                      left     optional chain
```

---

### 2.9 Desugaring Table

Command statements desugar to function calls before the compiler sees them.
The compiler never handles command keywords — only the parser does.

```
Source                                    Desugars to
──────────────────────────────────────    ──────────────────────────────────────
mouse 300, 400                            mouse.move(300, 400)
mouse 300, 400, 0.5                       mouse.move(300, 400, speed: 0.5)
click                                     mouse.click("left")
click right 100, 200                      mouse.click("right", 100, 200)
click 100, 200, 3                         mouse.click("left", 100, 200, count: 3)
drag 10, 10, 400, 300                     mouse.drag(10, 10, 400, 300)
drag 10, 10, 400, 300, 0.7               mouse.drag(10, 10, 400, 300, speed: 0.7)
scroll up 3                               mouse.scroll("up", 3)
hold left                                 mouse.hold("left")
release left                              mouse.release("left")
press ctrl+c                              keyboard.press("ctrl+c")
press enter                               keyboard.press("enter")
hold shift                                keyboard.hold("shift")
type "hello"                              keyboard.type("hello")
run "firefox"                             app.run("firefox")
open "report.pdf"                         app.open("report.pdf")
close "Firefox"                           app.close("Firefox")
focus "Notepad"                           window.focus("Notepad")
move "Notepad" 100, 100                   window.move("Notepad", 100, 100)
resize "Notepad" 800, 600                 window.resize("Notepad", 800, 600)
maximize "Notepad"                        window.maximize("Notepad")
minimize "Notepad"                        window.minimize("Notepad")
capture "shot.png"                        screen.capture("shot.png")
capture 0, 0, 1920, 1080 "r.png"         screen.capture("r.png", 0, 0, 1920, 1080)
wait 2s                                   time.wait(2s)
find button "Submit"                      ui.find("Submit", type: "button")
see "submit button"                       ui.see("submit button")
tap "Submit"                              ui.tap("Submit")
check found                               ui.check(found)
select "Option A" in found               ui.select(found, "Option A")
```

---

### 2.10 Complete Grammar (Consolidated)

```ebnf
(* ── Lexical ── *)
TERM     ::= NEWLINE | ';'
IDENT    ::= (LETTER | '_') ALPHA_NUM*   /* not a keyword */
INT_LIT  ::= DEC_INT | HEX_INT | BIN_INT | OCT_INT
FLOAT_LIT::= ...
DURATION ::= (INT_LIT | FLOAT_LIT) ('ms' | 's' | 'm' | 'h')
STRING_LIT ::= '"' STR_FRAGMENT* '"' | 'r"' RAW_CHAR* '"' | '"""' .* '"""'

(* ── Top level ── *)
program      ::= stmt_list EOF
stmt_list    ::= { stmt TERM }

(* ── Statements ── *)
stmt ::=
    'let'    ( bind_list '=' expr_list | '(' IDENT {',' IDENT} ')' '=' expr
             | '[' IDENT {',' IDENT} ']' '=' expr
             | '{' destruct_entry {',' destruct_entry} '}' '=' expr )
  | 'const'  IDENT '=' expr
  | 'fn'     IDENT '(' param_list? ')' block
  | 'if'     expr block { 'else' 'if' expr block } ( 'else' block )?
  | 'while'  expr block
  | 'repeat' expr ( 'as' IDENT )? block
  | 'for'    iter_bind 'in' iter_source block
  | 'match'  expr '{' NEWLINE match_arm* else_arm? '}'
  | 'try'    block catch_clause* else_clause? finally_clause?
  | 'use'    module_ref ( '{' IDENT {',' IDENT} '}' | 'as' IDENT )?
             { ',' module_ref ( '{' IDENT {',' IDENT} '}' | 'as' IDENT )? }
  | 'in'     STRING_LIT block
  | 'return' expr_list?
  | 'throw'  expr
  | 'break'
  | 'continue'
  | lvalue_list '=' expr_list       /* assignment */
  | lvalue aug_op expr              /* augmented assignment */
  | cmd_stmt                        /* automation command */
  | expr                            /* expression statement */

(* ── Control flow sub-productions ── *)
iter_bind    ::= IDENT | IDENT ',' IDENT
iter_source  ::= expr ( 'to' expr ( 'by' expr )? )?
match_arm    ::= 'case' pattern ( 'if' expr )? block
else_arm     ::= 'else' block
catch_clause ::= 'catch' IDENT ( 'if' expr )? block
else_clause  ::= 'else' block
finally_clause ::= 'finally' block
param_list   ::= param { ',' param }
param        ::= IDENT ( '=' expr )? | '*' IDENT
block        ::= '{' NEWLINE stmt_list '}'

(* ── Patterns ── *)
pattern ::= INT_LIT | FLOAT_LIT | STRING_LIT | BOOL_LIT | NONE_LIT
          | type_name | '(' pattern { ',' pattern } ')' | IDENT | '_'
type_name ::= 'int'|'float'|'string'|'bool'|'list'|'map'|'tuple'|'none'

(* ── Expressions (Pratt, low → high precedence) ── *)
expr         ::= pipe_expr ( 'if' pipe_expr 'else' pipe_expr )?
pipe_expr    ::= null_expr { '|>' null_expr }
null_expr    ::= or_expr   { '??' or_expr }
or_expr      ::= and_expr  { 'or' and_expr }
and_expr     ::= not_expr  { 'and' not_expr }
not_expr     ::= 'not' not_expr | cmp_expr
cmp_expr     ::= add_expr { (cmp_op | 'is' 'not'? | 'in' | 'not' 'in') add_expr }
cmp_op       ::= '==' | '!=' | '<' | '<=' | '>' | '>='
add_expr     ::= mul_expr { ('+' | '-') mul_expr }
mul_expr     ::= unary_expr { ('*' | '/' | '//' | '%') unary_expr }
unary_expr   ::= '-' unary_expr | pow_expr
pow_expr     ::= postfix_expr ( '**' unary_expr )?
postfix_expr ::= primary_expr { '(' arg_list? ')' | '[' expr ']' | '[' expr? 'to' expr? ']'
                              | '.' IDENT | '?.' IDENT }
arg_list     ::= arg { ',' arg }
arg          ::= IDENT ':' expr | expr
primary_expr ::= INT_LIT | FLOAT_LIT | BOOL_LIT | NONE_LIT | DURATION
               | STRING_LIT | IDENT | '(' expr ')'
               | '(' expr ',' { expr ',' }+ expr? ')'    /* tuple */
               | '[' ']' | '[' expr {',' expr} ','? ']'  /* list literal */
               | '[' expr comp_for+ ('if' expr)? ']'     /* list comp */
               | '{' '}' | '{' map_pair {',' map_pair} ','? '}'  /* map */
               | '{' expr ':' expr comp_for ('if' expr)? '}'     /* map comp */
               | 'set' '(' '[' expr {',' expr} ']' ')'           /* set */
               | 'fn' '(' param_list? ')' block                   /* fn expr */
               | 'match' expr '{' NEWLINE match_arm* else_arm? '}'  /* match expr */

comp_for  ::= 'for' iter_bind 'in' expr
map_pair  ::= (IDENT | STRING_LIT | INT_LIT) ':' expr

(* ── Lvalues ── *)
lvalue      ::= IDENT | postfix_expr '[' expr ']' | postfix_expr '.' IDENT
lvalue_list ::= lvalue { ',' lvalue }
bind_list   ::= IDENT { ',' IDENT }
expr_list   ::= expr { ',' expr }
aug_op      ::= '+=' | '-=' | '*=' | '/=' | '//=' | '%=' | '**=' | '??='
```
