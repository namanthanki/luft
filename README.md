# luft

A UCI chess engine written in C.

## Building

Requires a C11-compliant compiler (`gcc` or `clang`) and `make`.

```sh
make        # Standard optimized build (-O3 -flto)
make native # Native architecture build (-march=native)
```