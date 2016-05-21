# sat-shell

sat-shell is an interactive tcl-shell for solving satisfiability problems.

It uses tclln for the shell:
https://github.com/TestudoAquatilis/tclln.git

# Requirements

You need
- tclln
- glib version 2
- zlib
- minisat (or a compatible sat-solver)
- flex, bison, gcc, make, sed or something compatible for building.

# Build

For building the tool simply run:

> make

# Usage

For getting a list of available special commands in the shell type

> help

To see an example for usage there are tcl-scripts you can source in the shell with

> source examples/examples/sudoku.tcl

or execute directly with

> ./sat-shell --script examples/sudoku.tcl

It solves a sudoku puzzle and prints the solution.

# License

sat-shell is licensed under GPL.
