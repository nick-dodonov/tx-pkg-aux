#pragma once
#include <exception>

void sublib_throw_runtime_exception();
std::exception_ptr sublib_make_exception_ptr();
void sublib_log_exception_ptr(const std::exception_ptr& ep);
