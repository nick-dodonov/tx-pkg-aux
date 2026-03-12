#include "sublib.h"
#include "Log/Log.h"

#include <stdexcept>

void sublib_throw_runtime_exception()
{
	throw new std::runtime_error("sublib thrown runtime_error exception");
}

std::exception_ptr sublib_make_exception_ptr()
{
	const auto e = std::runtime_error("sublib made runtime_error exception");
	return std::make_exception_ptr(e);
}

void sublib_log_exception_ptr(const std::exception_ptr& ep)
{
	if (!ep) {
		Log::Info("nullptr");
		return;
	}
	try {
		std::rethrow_exception(ep);
	}
	catch (const std::exception& e) {
		Log::Info("'{}'", e.what());
	}
}
