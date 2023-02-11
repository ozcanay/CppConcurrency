#include <iostream>
#include <thread>
#include <future>

#include "../timer.h"

int foo(double, char, bool) {
    timeConsumingTask(3);
    return 42;
}

void operation()
{
    std::async( std::launch::async, timeConsumingTask, 2);
    std::async( std::launch::async, timeConsumingTask, 2);
    std::future<void> f{ std::async( std::launch::async, timeConsumingTask, 2 ) };
}

int main()
{
    Timer timer;

    operation();

    return 0;

    std::future<int> fut = std::async(std::launch::async, foo, 4.2, 'a', true);

    timeConsumingTask(5);

    std::cout << "Requesting the result via std::future..." << std::endl;
    int res = fut.get();

    std::cout << "Result: " << res << std::endl;
}