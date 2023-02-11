#include <iostream>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

class Timer {
public:
    Timer() {
        std::cout << "Timer started." << std::endl;
        start_ = std::chrono::high_resolution_clock::now();
    }

    ~Timer() {
        const auto finish = std::chrono::high_resolution_clock::now();
        std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::seconds>(finish - start_).count() << std::endl;
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

inline void timeConsumingTask(int duration) 
{
    std::this_thread::sleep_for(std::chrono::seconds(duration)); // emulates time consuming work.
}