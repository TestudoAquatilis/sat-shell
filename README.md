# sat-shell

sat-shell is an interactive tcl-shell for sat-solver interaction.

It is based on tclln for the shell:
https://github.com/TestudoAquatilis/tclln.git

# Requirements

You need
- tclln
- glib version 2
- zlib
- minisat (or a compatible sat-solver)
- flex, bison, gcc, make or something compatible for building.

# Build

For building the tool simply run:

> make

# Usage

To see an example for usage there is a tcl-script you can source in the shell with

> source examples/examples/zelda-puzzle.tcl

or execute directly with

> ./sat-shell examples/zelda-puzzle.tcl

It solves a puzzle of some probably well known game and prints the solution.

# License

sat-shell is licensed under GPL.
