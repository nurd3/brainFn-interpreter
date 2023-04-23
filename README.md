# brainFn-interpreter
**WARNING: THIS BRAINFN INTERPRETER IS YET TO HAVE CONCATENATION SUPPORT.**
<br>an interpreter for brainFn, learn about brainFn on the [Esolangs Wiki](https://esolangs.org/wiki/BrainFn)

# Run Guide
here is the general format for running the interpreter (using the command line):
```sh
./bfn (path) (tape size) (flags)
```
`path` - the path of the file to run<br>
`tape size` - the size of the tape (default is 65536)<br>
`flags` - explained below<br>
## Flags
`-log (path)` - where to log errors\/warnings, if not specified then errors and warnings print to the terminal.
`-debug` - makes the interpreter write debug information to the log file or the terminal, good for understanding how the interpreter runs through your code.
