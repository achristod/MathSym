# MathSym

MathSym is a simple scientific calculator and equation solver for up to
quadratic equations. It supports the four main operations in the real number
system namely addition, subtraction, multiplication, and division. The operands
need not just be scalars; they can be polynomials. However, the equation solver
will not solve for polynomials of degree higher than 2. Also, division with a
polynomial is not supported; only division by scalars.


## Requirements

The executable is called MathSym.exe, and it was built for Windows 10 using
Visual Studio 2019 Community Edition. It requires three files that configure
the modules of the application (described in detail in a following section).
These files are:

    tokenizer_config.txt
	parser_config.txt
	semantics_config.txt


## Examples

```bash
>> 1+2
ans = 3
```

```bash
>> (6-4)*(5+2)/(4*(4+6/3))
ans = 0.583333
```

```bash
>> (x-1)*(x-2)*(x-3)
ans = x^3 - 6x^2 + 11x - 6
```

```bash
>> (x-1)*(x-2)=0
x = 2 or x = 1
```

```bash
>> 0=3
No solutions
```

Note that MathSym selects between evaluating an expression and solving an
equation based on whether the = operator is present or not in the command.


## Design

The application consists of three main parts that evaluate a command: a
*tokenizer* (or lexical analyzer), a *parser*, and a *semantics analyzer*. The
tokenizer is implemented in the Tokenizer class, while the parser and the
semantics analyzer are both implemented in the Parser class. Below is a brief
description of each component.

### Tokenizer
  
This is responsible for breaking input into syntactic categories called tokens.
Each token has a category/type and a string representation (for example, 12.34
is a token of type number). The token types and are listed in the configuration
file tokenizer_config.txt, along with their respective matching regular
expressions. This approach enables a flexible application in which a new token
type can easily be inserted, without need to change the source code. The
regular expression are matched against the input command by means of the STL
library regex.

### Parser

This is responsible for parsing the stream of tokens and validating that it
conforms to a speicific set of grammar rules. These rules or productions are
listed in the configuration file parser_config.txt. The grammar expected by the
application is an LL(1) grammar, so these productions have to be maintained
right-recursive and backtrack-free. Otherwise the parser will not yield correct
results. The nonterminal symbols are the ones that exist on the left-hand side
of some production; all the other symbols in the productions are considered
nonterminal.

The application internally creates the FIRST and FOLLOW sets for each
nonterminal symbol, and then the FIRST+ sets for each production rule,
according to [1]. It then construct the LL(1) table that based on the current
symbol and the next input token, unambiguously picks the correct production to
expand.

After the above initialization phase of creating the LL(1) table, the parser is
ready to accept streams of tokens from the tokenizer and validate them against
the grammar. During that same parsing phase, it also creates a parse tree. This
tree, pruned and modified by the semantics component, will result into the
abstract syntax tree (AST) of the input command.

### Semantics Analyzer
    
This part is responsible for transforming the parse tree into an abstract
syntax tree, and then evaluating the expression recursively in a bottom-up
approach. For this it needs to know things like number of operands per
operator, or unused symbols in evaluation of expressions (for example, the
parentheses are used to denote the order of evaluation but do not play an
active role in the actual operations). All these are configurable in the file
semantics_config.txt. Its functionality is closely related to that of the
parser during the construction of the parse tree, hence their code is in the
same class.


## Compiling

The development and compilation of tis application was done entirely in
Microsoft Visual Studio 2019 Community Edition using the Microsoft compiler.
It uses some C++11 specific features like autos and lambdas, as well as the
standard regular expressions library <regex>. It does not use any third-party
libraries, including the Boost library, hence it will be portable to any
compiler that supports basic C++11 functionality.


## Future Work

The following are some items for additional features or future optimizations:

* Faster tokenization/parsing. Right now the regex library as implemented in 
Visual Studio 2019 is inefficient. Testing should be done with other regular
expression libraries like RE2, or alternatively we could write our own state
machine (but the later
approach would make it hard to have a fully configurable tokenization).
Also the parsing process could be sped up by eliminating dynamic memory
allocations, either with object pools or with custom-made allocators (in
fact I have implemented my own double-buffer allocator that I include
with this code in buffer_allocator.h, but it is not fully functional
according to the standard for allocators, and also it would require
careful integration into the code).
  
* Operator associativity. Right now, the grammar LL(1) hence it is
right-recursive. This implies that there is right preference in operator
associativity. For example, the expression 1-2+3 would be evaluted as
1-(2+3) = -4, instead of (1-2)+3 = 2. If there would be a demand for such a
feature, then an LR(1) parser would probably have to be considered, and that
would require almost a full rearchitecture of the parsing process.

* Support for exponentiation. Care should be taken though so that exponents are
all integers, or maybe real only when the expression is a a scalar.

* Multiplication without the * operator. For example, expressions like 2x as
they appear in the mathematics literature are not supported.

* Numberical IDs to terminal and nonterminal symbols. This would enable faster
lookup in the LL(1) table, but not too much to be considered a high-priority
feature.

* Serialize object like the LL(1) table and save them to a file. This would
speed up the initialization of the application, where the FIRST, FOLLOW, and
FIRST+ sets are computed before the LL(1) table construction. Other things like
semantics configuration can also be save into a binary form in a file for
faster application startup.


## References

[1] Engineering: A Compiler, K. Cooper and L. Torczon, Morgan Kaufmann, 2011

[2] Introduction to Automata Theory, Languages, and Computation, J. E.
    Hopcroft, R. Motwani, and J. D. Ullman, Pearson, 2006


## Versioning

1.0


## Creators
[Antonis Christodoulou](christan305@hotmail.com)
