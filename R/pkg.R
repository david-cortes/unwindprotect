call_R_function_with_unwind_protect <- function(R_fun) {
    return(.Call(call_R_fun_w_unwind_protect, R_fun))
}

call_R_function_without_unwind_protect <- function(R_fun) {
    return(.Call(call_R_fun_wo_unwind_protect, R_fun))
}
