#include <R.h>
#include <Rinternals.h>

#include <exception>
#include <cassert>

/* Example C++ object which prints messages when it is constructed and destructed */
class CustomCppClass
{
public:
    CustomCppClass() {
        Rprintf("C++ custom object is being constructed.\n");
    }
    ~CustomCppClass() {
        Rprintf("C++ custom object is being destructed.\n");
    }
};

/* Mechanism for using R's unwind protection with continuation tokens to trigger C++ stack unwinding will be as follows:
   - An R continuation token will be created at the beginning of a C++ function call triggered
     by '.Call' from R.
   - A function returning an R object will be called through the 'R_UnwindProtect' wrapper,
     in a C++ try-catch block, which will be passed this continuation token.
   - There will be a custom C++ exception class, which will be thrown from the R function
     that gets called by the unwind-protector call when an R error happens.
   - The C++ exception will be catched from a try-catch block from outside, from where the
     'R_UnwindProtect' call will be triggered.
   - The 'catch' part of the block will make sure that everything C++ that was allocated in the 'try'
     block gets its destructor called, as it will be entered into when the exception is thrown.
   - Inside of the 'catch' block, R's unwind continuation will be called with the same continuation
     token in order to properly issue the R error from outside. */
struct Exception_Signaling_R_error : public std::exception {};

void throw_exception_from_R_error(void *unused, Rboolean jump)
{
    assert(unused == nullptr);
    
    if (jump) {
        /* This will be called right before an R error is thrown */
        Rprintf("Converting R error to C++ exception\n");
        throw Exception_Signaling_R_error();
    }
}

SEXP wrapped_R_function_call(void *ptr_to_R_function)
{
    SEXP R_fun = *(SEXP*)ptr_to_R_function;
    SEXP R_expr_do_call = PROTECT(Rf_install("do.call"));
    SEXP R_empty_list = PROTECT(Rf_allocVector(VECSXP, 0));
    SEXP R_expr_fun_call = PROTECT(Rf_lang3(R_expr_do_call, R_fun, R_empty_list));
    Rprintf("Will call supplied R function\n");
    SEXP fun_result = PROTECT(Rf_eval(R_expr_fun_call, R_GlobalEnv));
    Rprintf("Done with call to R function\n");
    UNPROTECT(4);
    return fun_result;
}

SEXP safe_R_function_call(SEXP R_fun, SEXP continuation_token)
{
    return R_UnwindProtect(
        wrapped_R_function_call, (void*)&R_fun, /* 1st arg is a function that gets called with 2nd arg */
        throw_exception_from_R_error, nullptr, /* 3rd arg is a function that gets called with 4th arg */
        continuation_token
    );
}

extern "C" {

SEXP call_R_fun_w_unwind_protect(SEXP R_fun)
{
    SEXP continuation_token = PROTECT(R_MakeUnwindCont());
    try {
        CustomCppClass cpp_obj{};
        SEXP out = PROTECT(safe_R_function_call(R_fun, continuation_token));
        UNPROTECT(2);
        return out;
    }

    catch (const Exception_Signaling_R_error &ex) {
        R_ContinueUnwind(continuation_token); 
    }

    catch (const std::exception &ex) {
        /* This shouldn't happen in this example, but is nevertheless a good practive to have */
        Rf_error("A C++ exception ocurred: %s\n", ex.what());
    }
    
    /* Code below will never be reached */
    Rprintf("Unreachable code section - you should not be seeing this.\n");
    UNPROTECT(1);
    return R_NilValue;
}

SEXP call_R_fun_wo_unwind_protect(SEXP R_fun)
{
    CustomCppClass cpp_obj{};
    return wrapped_R_function_call((void*)&R_fun);
}

/* Registering the functions */
static const R_CallMethodDef callMethods [] = {
    {"call_R_fun_w_unwind_protect", (DL_FUNC) &call_R_fun_w_unwind_protect, 1},
    {"call_R_fun_wo_unwind_protect", (DL_FUNC) &call_R_fun_wo_unwind_protect, 1},
    {NULL, NULL, 0}
};

void R_init_unwindprotect(DllInfo *info)
{
    R_registerRoutines(info, NULL, callMethods, NULL, NULL);
    R_useDynamicSymbols(info, TRUE);
}

} /* extern "C" */
