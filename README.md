# CLOX

This is the C implementation of Lox language from Robert Nystrom's amazing book [Crafting Interpreters](https://www.craftinginterpreters.com).

## Differences from the original book

- Ordering can be different, so forward declarations are also different
- There are some comments in between which help in reminding the explanations from the book

> I tried to write most of the code by hand, so there may be small mistakes. Commits are present chapter-wise, which can help to trace the progress in each chapter.

## How to run

The Makefile assumes gcc compiler, but any other can be used.

```bash
make # builds the source and puts final executable in bin/
```

To clear the bin folder

```bash
make clean
```

I could not add proper dependencies in Makefile, so run this command on making any change

```bash
make rebuild
```

To directly run the REPL

```bash
make run
```

To run another lox file

```bash
make rebuild
cd bin/
./clox script.lox # script.lox is the name of lox file
```

## Thanks

Thanks to [Robert Nystrom](https://twitter.com/intent/user?screen_name=munificentbob) for providing the book with beginner friendly explanation and code for every single line which helped in clarifying so many topic related to programming, data structures and compilers and interpreters
