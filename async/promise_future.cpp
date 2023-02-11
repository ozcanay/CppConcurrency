#include <iostream>
#include <future>
#include <exception>
#include <stdexcept>

int test();

int main()
{
    try
    {
        return test();
    }
    catch (std::future_error const & e)
    {
        std::cout << "Future error: " << e.what() << " / " << e.code() << std::endl;
    }
    catch (std::exception const & e)
    {
        std::cout << "Standard exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Unknown exception." << std::endl;
    }
}


int test()
{
    std::promise<int> pr;
    auto fut = pr.get_future();

    {
        std::promise<int> pr2(std::move(pr));
        pr2.set_value(10);
    }

    std::cout << "Result: " << fut.get() << std::endl;

    return 0;
}