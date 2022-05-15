#include <boost/asio.hpp>

#include <chrono>
#include <functional>

namespace
{
// Set the read loop interval to be 1 second for now
// TODO: Make this a configuration option
static constexpr std::chrono::seconds readIntervalSec(1);
} // namespace

// boost::async_wait requires `const boost::system::error_code&` parameter
void readLoop(boost::asio::steady_timer* t, const boost::system::error_code&)
{
    /* This will run every readIntervalSec second for now */
    t->expires_from_now(readIntervalSec);
    t->async_wait(std::bind_front(readLoop, t));
}

int main()
{
    boost::asio::io_context io;
    boost::asio::steady_timer t(io, readIntervalSec);

    t.async_wait(std::bind_front(readLoop, &t));

    io.run();

    return 0;
}
