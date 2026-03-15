#include "Boot/Boot.h"

#include <boost/http.hpp>
#include <iostream>

using namespace boost::http;

int main(const int argc, const char** argv)
{
    Boot::DefaultInit(argc, argv);

    // Build a request
    request req(method::get, "/api/users");
    req.set(field::host, "example.com");
    req.set(field::accept, "application/json");

    // Build a response
    response res(status::ok);
    res.set(field::content_type, "application/json");
    res.set(field::content_length, "42");

    // Access the serialized form
    std::cout << req.buffer();
    std::cout << res.buffer();

    std::cout << std::flush;

    return 0;
}
