# brainFn-interpreter
<br>An interpreter for brainFn, learn about brainFn on the [Esolangs Wiki](https://esolangs.org/wiki/BrainFn)
# Todo List
* brainFn validator
* fix script calls
* function stack (might be scrapped)

# Run Guide
here is the general format for running the interpreter (using the command line):
```sh
./bfn (path) (tape size) (flags)
```
`path` - the path of the file to run<br>
`tape size` - the size of the tape (default is 65536)<br>
`flags` - explained below<br>
## Flags
`-log (path)` - where to log errors\/warnings.<br>
`-echo (on|off)` - if marked as on, then errors\/warnings, as well as debug info will print to the terminal.<br>
`-debug` - makes the interpreter write debug information to the log file and or the terminal, good for understanding how the interpreter runs through your code.<br>
