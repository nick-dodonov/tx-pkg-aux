#include <boost/capy/task.hpp>

namespace Coro
{
    template<typename T = void>
    using Task = boost::capy::task<T>;
}
