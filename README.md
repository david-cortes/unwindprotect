# R Unwind Protection Example

This is an example of using the R unwind protection system (e.g. function `R_UnwindProtect`) in C++ code called from R, in order to guarantee that destructors of C++ objects get called through a proper stack unwind before the R error triggers a C long jump.

# Motivation

When using R's C API through `.Call` and similar, R errors are handled through long jumps in C function calls, which simply discard the function call stack without a proper stack unwind like in normal function returns. This is fine when one allocates objects using R's own memory allocators, but it doesn't play along with objects allocated from other allocators.

If one allocates a C++ object during the execution of some function - e.g.:
```c++
SEXP cpp_function_for_Rs_Call()
{
    CppObject obj();
    ...
    return ...
}
```

One usually expects that the destructor of this object will get called before the function returns, and so any resource that the object might have taken (e.g. a memory allocation) will be properly cleaned up during its destruction.

However, if an R error happens while using functions from R's C API - e.g.
```c++
SEXP cpp_function_for_Rs_Call()
{
    CppObject obj();
    ...
    SEXP R_obj = Rf_eval(R_expr, R_GlobalEnv); /* Assume this throws an R error */
    ...
    return ...
}
```

Then a long jump is triggered which **bypasses the destructor of the C++ objects**, usually leading to memory leaks and other forms of resource leaks.

In later versions of R, a C function `R_UnwindProtect` was introduced, which needs to be used with a so-called "continuation token", and this function can be used to trigger a C++ exception that can in turn be used to trigger a stack unwind in a try-catch block.

See the C++ file under `/src` for the example usage of this `R_UnwindProtect` function.

# What this repository contains

This repository contains an example of an R package in which two functions are offered:

* `call_R_function_with_unwind_protect`
* `call_R_function_without_unwind_protect`

Both of these functions:

* Take as argument an R function.
* Use `.Call` to call a C++ function, wich in turn calls the R function supplied there (without arguments) through R's C API.
* Before calling the R function, both of them create a C++ object which prints messages when it is consructed and when it is destructed.

One of these functions uses unwind protection with the continuation token system, while the other one doesn't.

Hence, if one passes an R function which generates an error when called, one can observe differences in behavior between the two functions: one of them will print the C++'s object destructor message (indicating that the destructor was called), while the other won't (indicating that the destructor was not called and the memory was thus leaked).

For details, see the C++ file under `/src`.

# Try it out

First, install the package with:

```r
remotes::install_github("david-cortes/unwindprotect")
```

Then, run some sample code using both of these functions to call a function which doesn't error out:

```r
library(unwindprotect)

function_without_errors <- function() {
    cat("Good function is being called\n")
    return("Good")
}
call_R_function_with_unwind_protect(function_without_errors)
call_R_function_without_unwind_protect(function_without_errors)
```

Then see what happens when passing a function that _does_ generate an R error:
```r
function_with_errors <- function() {
    cat("Bad function is being called\n")
    stop("An R error")
    return("Bad")
}
call_R_function_with_unwind_protect(function_with_errors)
call_R_function_without_unwind_protect(function_with_errors)
```

Verify that nothing happens when calling `gc` either - i.e. memory is leaked:
```r
call_R_function_without_unwind_protect(function_with_errors)
gc()
```
