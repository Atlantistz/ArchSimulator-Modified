#ifndef SIMULATOR_DEBUG_H
#define SIMULATOR_DEBUG_H

#include <cstdio>

/* mocked from NEMU */

// macro testing
#define CHOOSE2nd(a, b, ...) b
#define MUX_WITH_COMMA(contain_comma, a, b) CHOOSE2nd(contain_comma a, b)
#define MUX_MACRO_PROPERTY(p, macro, a, b) \
  MUX_WITH_COMMA(concat(p, macro), a, b)

// macro concatenation
#define concat_temp(x, y) x##y
#define concat(x, y) concat_temp(x, y)

// define placeholders for some property
#define __P_DEF_0 X,
#define __P_DEF_1 X,

// define some selection functions based on the properties of BOOLEAN macro
#define MUXDEF(macro, X, Y) MUX_MACRO_PROPERTY(__P_DEF_, macro, X, Y)
#define MUXNDEF(macro, X, Y) MUX_MACRO_PROPERTY(__P_DEF_, macro, Y, X)

// simplification for conditional compilation
#define __IGNORE(...)
#define __KEEP(...) __VA_ARGS__

// keep the code if a boolean macro is defined
#define IFDEF(macro, ...) MUXDEF(macro, __KEEP, __IGNORE)(__VA_ARGS__)
// keep the code if a boolean macro is undefined
#define IFNDEF(macro, ...) MUXNDEF(macro, __KEEP, __IGNORE)(__VA_ARGS__)

#endif
