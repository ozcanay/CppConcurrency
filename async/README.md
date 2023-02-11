Asynchronous providers (writers, producers) from highest level to lowest:

1. ```std::async```
2. ```std::packaged_task```
3. ```std::promise```

- ```std::async``` is a higher-level convenience utility that gives you an asynchronous result object and internally takes care of creating the asynchronous provider and making the shared state ready when the task completes. 
- You could emulate ```std::async``` with a ```std::packaged_task``` (or ```std::bind``` and a ```std::promise```) and a ```std::thread``` --> SHOW THIS IN CODE.
- It's safer and easier to use ```std::async``` rather than inventing it again.

Demonstrating these async providers:

```
#include <iostream>
#include <future>
#include <thread>
 
int main()
{
    // future from a packaged_task
    std::packaged_task<int()> task([](){ return 7; }); // wrap the function
    std::future<int> f1 = task.get_future();  // get a future
    std::thread(std::move(task)).detach(); // launch on a thread
 
    // future from an async()
    std::future<int> f2 = std::async(std::launch::async, [](){ return 8; });
 
    // future from a promise
    std::promise<int> p;
    std::future<int> f3 = p.get_future();
    std::thread( [](std::promise<int>& p){ p.set_value(9); }, 
                 std::ref(p) ).detach();
 
    std::cout << "Waiting...";
    f1.wait();
    f2.wait();
    f3.wait();
    std::cout << "Done!\nResults are: "
              << f1.get() << ' ' << f2.get() << ' ' << f3.get() << '\n';
}
```

## Output:

```
Waiting...Done!

Results are: 7 8 9
```

Note that ```std::future::get()``` also calls ```std::future::wait()``` internally.

# std::async

```
template< class Function, class... Args >
[[nodiscard]] std::future<std::invoke_result_t<std::decay_t<Function>, std::decay_t<Args>...>>
async(std::launch policy, Function&& f, Args&&... args);
```

```std::async``` runs a function asynchronously (potentially in a new thread) and returns a ```std::future``` that will hold the result.

The function template ```std::async``` runs the function f asynchronously (potentially in a separate thread which might be a part of a thread pool) and returns a ```std::future``` that **will eventually hold** the result of that function call.

```std::async``` can conceptually (this is not mandated by the standard) understood as a function that creates a ```std::promise```, pushes that into a thread pool (of sorts, might be a thread pool, might be a new thread, ...) and returns the associated ```std::future``` to the caller. On the client side you would wait on the ```std::future``` and a thread on the other end would compute the result and store it in the ```std::promise```. Note: the standard requires the shared state and the ```std::future``` but not the existence of a ```std::promise``` in this particular use case.

There were two main arguments that caused the current design where ```~std::future``` blocks **if the future was
produced by ```std::async```**.

1. First, it was felt that because the returned future was the only handle to the async operation, if it did not
block then there would be no way to join with the async operation, leading to a detached task that could
run beyond the end of main off into static destruction time, which was considered anathema.

2. Similarly, it was felt that it was too easy for a lambda spawned by an async operation to capture a local
variable by reference but then outlive the function.

One (incomplete) way to implement ```std::async``` using ```std::promise``` could be:

```
template<typename F>
auto async(F&& func) -> std::future<decltype(func())>
{
    typedef decltype(func()) result_type;

    auto promise = std::promise<result_type>();
    auto future  = promise.get_future();

    std::thread(std::bind([=](std::promise<result_type>& promise)
    {
        try
        {
            promise.set_value(func()); // Note: Will not work with std::promise<void>. Needs some meta-template programming which is out of scope for this question.
        }
        catch(...)
        {
            promise.set_exception(std::current_exception());
        }
    }, std::move(promise))).detach();

    return std::move(future);
}
```

Other way to implement ```std::async``` using ```std::packaged_task```:

```
template<typename F>
auto async(F&& func) -> std::future<decltype(func())>
{
    auto task   = std::packaged_task<decltype(func())()>(std::forward<F>(func));
    auto future = task.get_future();

    std::thread(std::move(task)).detach();

    return std::move(future);
}
```

Without ```std::future``` and ```std::async```, you would normally need a ```std::mutex``` and modify shared data in order to return a result from the thread. With ```std::async``` and ```std::future``` this is not needed at all and very easy.

```std::async``` is the default launcher and the cornerstone of composability.

Two policies:

1. ```std::launch::async```
2. ```std::launch::deferred```

Note also that ```std::async``` can be launched with policies ```std::launch::async``` or ```std::launch::deferred``` (```std::launch::async | std::launch::deferred```). This is the default option.

```std::async``` is designed to provide a **synchronization point** so you can get the result of the function being evaluated asynchronously.

If you use the ```std::launch::async``` **policy** then it will certainly run asynchronously in a new thread (However, there are a few implementations that run in the current thread without creating a new one if the behavior is indistinguishable). If you don't specify a policy it *might* run in a new thread. 

Other policy type is ```std::launch::deferred``` and it is lazy evaluation in the calling thread. ```std::launch::deferred``` indicates that the function call is to be **deferred** until either ```wait()``` or ```get()``` is called on the ```std::future```. Ownership of the ```std::future``` *can be transferred to another thread* before this happens.

If you don't specify the launch policy then a smart implementation can decide whether to start a new thread, or return a deferred function, or return something that decides later, when more resources may be available.

It only blocks if foo hasn't completed, but if it was run asynchronously (e.g. because you use the std::launch::async policy) it might have completed before you need it.

We have very little control over the details. In particular, we don't even know if the function is exe­cu­ted concurrently, serially upon ```get()```, or by some other black magic.

```std::future``` will only blocks if the result isn't already ready.

```std::async``` returns ```std::future```, not ```std::promise```.

Using ```std::async``` is a convenient way to fire off a thread for some asynchronous computation and marshal the result back via a ```std::future```.

Currently, ```std::async``` is probably best suited to handling either **very long** running computations or long running IO for fairly simple programs. It doesn't guarantee low overhead though (and in fact the way it is specified makes it difficult to implement with a thread pool behind the scenes), so it's not well suited for **finer grained** workloads. For that you either need to roll your own thread pools using ```std::thread```.

## Where std::async lacks

- Sometimes you cannot use ```std::async``` when you don't want to set the ```std::future```'s value by returning from the task running in a separate thread. This can be the case when you want to use **two or more futures from one thread**. This wouldn't be possible with ```std::async```. In that case you can use a pair or multiple pairs of ```std::promise``` and ```std::future```.
- Also keep in mind that the returned future in std::async has a special shared state, which demands that future::~future blocks. This is not the case for ```std::packaged_task```.

## Code snippets

This is the function we will be calling throughout the demonstration:

```
int foo(double, char, bool) {
    timeConsumingTask(3);
    return 42;
}
```

The most trivial example:

```
Timer timer;

std::future<int> fut = std::async(foo, 4.2, 'a', true);

std::cout << "Requesting the result via std::future..." << std::endl;
int res = fut.get();

std::cout << "Result: " << res << std::endl;
```

Here, the benefit of ```std::async``` is not obvious. The application lasts for 3 seconds, and mostly waits blocked for getting a ```std::future```. This ```std::future``` will not be available till the time consuming task is done for. We will now modify the code a little bit to emphasize the benefit of ```std::async```.

```
Timer timer;

std::future<int> fut = std::async(std::launch::async, foo, 4.2, 'a', true);

timeConsumingTask(5);

std::cout << "Requesting the result via std::future..." << std::endl;
int res = fut.get();

std::cout << "Result: " << res << std::endl;
```

### Output:

```
Timer started.
Requesting the result via std::future...
Result: 42
Elapsed time: 5
```

Here, we can now observe the benefit of ```std::async```. By the time the main thread finishes its time consuming task of 5 seconds, ```std::async``` will have processed ```foo``` function and therefore, we will not be blocking to request the result from the ```std::future``` object. In this case, ```std::async``` has decided to execute ```foo``` function in a separate thread.

If we force ```std:async``` to lazy evaluate, the task will be executed on the calling (main) thread the first time its result is requested via ```std::future```.

```
Timer timer;

std::future<int> fut = std::async(std::launch::deferred, foo, 4.2, 'a', true);

timeConsumingTask(5);

std::cout << "Requesting the result via std::future..." << std::endl;
int res = fut.get();

std::cout << "Result: " << res << std::endl;
```

### Output:
```
Timer started.
Requesting the result via std::future...
Result: 42
Elapsed time: 8
```

We can confirm that ```foo``` has **not** been executed in a separate thread by ```std::async```, because the total elapsed time is 8 seconds.

## Common Pitfalls

```
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
}
```

This misleading code can make you think that the ```std::async``` calls are asynchronous, they are actually synchronous. The ```std::future``` instances returned by ```std::async``` are temporary and will block **because their destructor is called** right when ```std::async``` returns as they are not assigned to a variable.

The first call to ```std::async``` will block for 2 seconds, followed by another 2 seconds of blocking from the second call to ```std::async```. We may think that the last call to ```std::async``` does not block, since we store its returned ```std::future``` instance in a variable, but since that is a local variable that is destroyed at the end of the scope, it will actually block for an additional 2 seconds at the end of the scope of the function, when local variable f is destroyed.

### Output:
```
Timer started.
Elapsed time: 6
```

## Relevant Links

https://stackoverflow.com/questions/17963172/why-should-i-use-stdasync

https://stackoverflow.com/questions/25814365/when-to-use-stdasync-vs-stdthreads?noredirect=1&lq=1

https://stackoverflow.com/questions/30810305/confusion-about-threads-launched-by-stdasync-with-stdlaunchasync-parameter

https://stackoverflow.com/questions/9733432/the-behavior-of-stdasync-with-stdlaunchasync-policy?rq=1

# std::packaged_task

```std::packaged_task``` packages a function to store its return value for asynchronous retrieval.

The class template ```std::packaged_task``` wraps any Callable target (function, lambda expression, bind expression, or another function object) so that it can be invoked asynchronously. Its return value or exception thrown is stored in a shared state which can be accessed through ```std::future``` objects.

A packaged_task won't start on it's own, you have to invoke it. Producer thread will invoke it, consumer thread creates it, but does not invoke it, defers the invocation.


```std::packaged_task``` is a helper around ```std::promise```

- ```std::packaged_task``` is a template that wraps a function and provides a future for the functions return value.
- ```std::packaged_task``` is call­able, and calling it is at the user's discretion. We can set it up like this:

```
std::packaged_task<int(double, char, bool)> tsk(foo);
std::future<int> fut = tsk.get_future();
```

The future becomes ready once we call the task and the call completes. This is the ideal job for a se­pa­rate thread. We just have to make sure to **move the task into the thread**:

```
std::thread thr(std::move(tsk), 1.5, 'x', false);
```

The thread starts running immediately. We can either detach it, or have join it at the end of the scope. Whenever the function call finishes, our result is ready:

```
int res = fut.get();
```

#

```
std::function<int(int,int)> f { add };
```

Once we have f, we can execute it, in the same thread, like:

```
int result = f(1, 2); //note we can get the result here
```

Or, in a different thread, like this:

```
std::thread t { std::move(f), 3, 4 };
t.join(); 
```
If we see carefully, we realize that executing ```f``` in a different thread creates a new problem: how do we get the result of the function? Executing ```f``` in the same thread does not have that problem — we get the result as returned value, but when executed it in a different thread, we don't have any way to get the result. **That is exactly what is solved by ```std::packaged_task```**.

Fundamentally ```std::function``` and ```std::packaged_task``` are similar kind of thing: they simply wrap callable entity, with one difference: ```std::packaged_task``` is multithreading-friendly, because it provides a channel through which it can pass the result to other threads. Both of them do **NOT** execute the wrapped callable entity by themselves. One needs to invoke them, either in the same thread, or in another thread, to execute the wrapped callable entity. So basically there are two kinds of thing in this space:

- what is executed i.e regular functions, ```std::function```, ```std::packaged_task```, etc.
- how/where is executed i.e threads, thread pools, executors, etc.

## Example std::packaged_task Implementation

```
template <typename> class my_task;

template <typename R, typename ...Args>
class my_task<R(Args...)>
{
    std::function<R(Args...)> fn;
    std::promise<R> pr;             // the promise of the result
public:
    template <typename ...Ts>
    explicit my_task(Ts &&... ts) : fn(std::forward<Ts>(ts)...) { }

    template <typename ...Ts>
    void operator()(Ts &&... ts)
    {
        pr.set_value(fn(std::forward<Ts>(ts)...));  // fulfill the promise
    }

    std::future<R> get_future() { return pr.get_future(); }

    // disable copy, default move etc.
};
```

```std::packaged_task```:
- Stores the callable object.
- Creates a promise with the type of the function's return value.
- Provides a means to both get a future and to execute the function (call operator above) that generates the value.

When and where the task actually gets executed is none of packaged_task's business.

## std::async vs std::packaged_task

- By using ```std::async``` you cannot run your task on a specific thread anymore, where ```std::packaged_task``` can be moved to other threads.
- A ```std::packaged_task``` needs to be invoked before you call ```std::future::get()```, otherwise you program will freeze as the future will never become ready.
- Use ```std::async``` if you want some things done and don't really care when they're done, and ```std::packaged_task``` if you want to **wrap** up things in order to move them to other threads or **call them later** (std::packaged_task is for deferring!).
- In the end a ```std::packaged_task``` is just a lower level feature for implementing ```std::async``` (which is why it can do more than ```std::async``` if used together with other lower level stuff, like ```std::thread```). 
- Simply spoken, a ```std::packaged_task``` is a ```std::function``` linked to a ```std::future``` and ```std::async``` wraps and calls a ```std::packaged_task``` (possibly in a different thread).


# std::promise & std::future

- ```std::future``` is an asynchronous return object ("an object that **reads (reader, consumer)** results from a shared state") and a ```std::promise``` is an asynchronous provider ("an object that **provides (writer, producer)** a result to a shared state")
- In other words, a promise is the thing that you set a result on, so that you can ```get()``` the result from the *associated* future. (So, it is implied here that promises and futures are associated, promise is the writer end, whereas future is the reader end.)
- Shared state is shared between promise and its associated future.
- PROMISE --> FUTURE (future is constructed by a promise!)
- ```std::promise``` is used by the "producer/writer" of the asynchronous operation.
- ```std::future``` is used by the "consumer/reader" of the asynchronous operation.
- The reason it is separated into these two separate "interfaces" is to hide the "write/set" functionality from the "consumer/reader".

#

- ```std::promise``` is non-copyable. You can move it, but you can't copy it. ```std::future``` is also non-copyable, but a ```std::future``` can become a ```std::shared_future``` that is copyable. So you can have multiple destinations, but only one source.
- ```std::promise``` can only set the value; it can't even get it back. 
- ```std::future``` can only get the value; it cannot set it. Therefore, we have an asymmetric interface.

#

C++11 futures are the caller's end of a communications channel that begins with a callee that's (typically) called asynchronously. When the called function has a result to communicate to its caller, it performs a set operation on the ```std::promise``` corresponding to the future.  That is, an asynchronous callee sets a promise (i.e., writes a result to the communication channel between it and its caller), and its caller gets the future (i.e., reads the result from the communications channel).

#

Between the time a callee sets its promise and its caller does a corresponding get, an arbitrarily long time may elapse. (In fact, the get may never take place, but that's a detail I'm ignoring.) As a result, the std::promise object that was set may be destroyed before a get takes place.  This means that the value with which the callee sets the promise can't be stored in the promise--the promise may not have a long enough lifetime.  The value also can't be stored in the future corresponding to the promise, because the std::future returned from std::async could be moved into a std::shared_future before being destroyed, and the std::shared_future could then be copied many times to new objects, some of which would subsequently be destroyed. In that case, which future would hold the value returned by the callee?

Because neither the promise nor the future ends of the communications channel between caller and callee are suitable for storing the result of an asynchronously invoked function, it's stored in a neutral location. This location is known as the **shared state**.  There's nothing in the C++ standard library corresponding to the shared state.  No class, no type, no function. In practice, I'm guessing it's implemented as a class that's templatized on at least the type of the result to be communicated between callee and caller.


## std::future

```std::future``` waits for a value that is set asynchronously.

The class template ```std::future``` provides a mechanism to access the result of asynchronous operations:

- An asynchronous operation (created via ```std::async```, ```std::packaged_task```, or ```std::promise```) can provide a ```std::future``` object to the creator of that asynchronous operation.
- The creator of the asynchronous operation can then use a variety of methods to query, wait for, or extract a value from the ```std::future```. These methods **may** block if the asynchronous operation **has not yet** provided a value.
- When the asynchronous operation is ready to send a result to the creator, it can do so by modifying shared state (e.g. ```std::promise::set_value```) that is **linked to** the creator's ```std::future```.
- ```std::future``` references shared state that is **not** shared with any other asynchronous return objects (as opposed to ```std::shared_future```).

- The good news is that the Standard’s specification of future and shared_future destructors specifies that they never block. This is vital. The bad news is that the Standard specifies that the associated state of an operation launched by ```std::async (only!)``` does nevertheless cause future destructors to block.

## std::promise

```std::promise``` stores a value for asynchronous retrieval.

-  The class template ```std::promise``` provides a facility to store a value *or an exception* that is later acquired asynchronously via a ```std::future``` object created by the ```std::promise``` object.
- ```std::promise``` object is meant to be used only once (one-shot!)
- Each ```std::promise``` is associated with a shared state, which contains some state information and a result which may be not yet evaluated, evaluated to a value (possibly void) or evaluated to an exception. A promise may do three things with the shared state:
    - **make ready**: the promise stores the result or the exception in the shared state. Marks the state ready and unblocks any thread waiting on a future associated with the shared state.
    - **release**: the promise gives up its reference to the shared state. If this was the last such reference, the shared state is destroyed. Unless this was a shared state created by ```std::async``` which is not yet ready, this operation does not block.
    - **abandon**: the promise stores the exception of type ```std::future_error``` with error code ```std::future_errc::broken_promise```, makes the shared state ready, and then releases it.
- The promise is the "push" end (**promise is the writer!**) of the promise-future communication channel; it stores a value in the shared state to be read from a mechanism such as ```std::future::get```.


#

The promise is the building block for communicating with a future. The principal steps are these:

1. The calling thread makes a promise.
2. The calling thread obtains a future from the promise.
3. The **promise**, along with function arguments, are moved into a separate thread.
4. The new thread executes the function and fulfills the promise.
5. The original thread retrieves the result.

So, newly created thread will produce a value to be read from the calling thread via future.

#

Promises are intimately related to exceptions. The interface of a promise alone is not enough to convey its state completely, so exceptions are thrown whenever an operation on a promise does not make sense. All exceptions are of type ```std::future_error```, which derives from ```std::logic_error```. First off, a description of some constraints:

- A default-constructed promise is inactive. Inactive promises can die without consequence.
- A promise becomes active when a future is obtained via ```get_future()```. However, **only one future may be obtained**!
- A promise must either be satisfied in 2 ways:
    - have a value set via ```set_value()``` 
    - have an exception set via ```set_exception()``` 
before its lifetime ends if its future is to be consumed. A satisfied promise can die without consequence, and ```get()``` becomes available on the future. 
- A promise with an exception will raise the stored exception upon call of ```get()``` on the future. 
- If the promise dies with neither value nor exception, calling ```get()``` on the future will raise a "broken promise" exception.

#

- It is possible to query ```std::future::valid()``` (or use ```std::future::wait_for/wait_until(...)```) to know when it is done.
- ```std::future``` object **WILL** eventually hold the return value, this is why it is called "future".

## Demonstration

Harness:

```
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
```

### Case 1: Inactive promise

```
int test()
{
    std::promise<int> pr;
    return 0;
}
// fine, no problems
```

### Case 2: Active promise, unused

```
int test()
{
    std::promise<int> pr;
    auto fut = pr.get_future();
    return 0;
}
// problem: fut.get() would block indefinitely
```

### Case 3: Too many futures
```
int test()
{
    std::promise<int> pr;
    auto fut1 = pr.get_future();
    auto fut2 = pr.get_future();  //   Error: "Future already retrieved"
    return 0;
}
```

### Case 4: Satisfied promise
```
int test()
{
    std::promise<int> pr;
    auto fut = pr.get_future();

    {
        std::promise<int> pr2(std::move(pr));
        pr2.set_value(10);
    }

    return fut.get();
}
// Fine, returns "10".
```

### Case 5: Too much satisfaction
```
int test()
{
    std::promise<int> pr;
    auto fut = pr.get_future();

    {
        std::promise<int> pr2(std::move(pr));
        pr2.set_value(10);
        pr2.set_value(10);  // Error: "Promise already satisfied"
    }

    return fut.get();
}
```

## Case 6: Broken promise
```
int test()
{
    std::promise<int> pr;
    auto fut = pr.get_future();

    {
        std::promise<int> pr2(std::move(pr));
    }   // Error: "broken promise"

    return fut.get();
}
```

## Relevant Links

https://stackoverflow.com/questions/11004273/what-is-stdpromise

https://stackoverflow.com/questions/12620186/futures-vs-promises?noredirect=1&lq=1

https://stackoverflow.com/questions/34169602/why-we-need-both-stdpromise-and-stdfuture?noredirect=1&lq=1

https://stackoverflow.com/questions/14283703/when-is-it-a-good-idea-to-use-stdpromise-over-the-other-stdthread-mechanisms?noredirect=1&lq=1

https://stackoverflow.com/questions/25878765/stdpromise-and-stdfuture-in-c?noredirect=1&lq=1

https://stackoverflow.com/questions/63843676/why-stdfuture-is-different-returned-from-stdpackaged-task-and-stdasync?noredirect=1&lq=1 --> this seems like a cool question.

https://stackoverflow.com/questions/37362865/why-is-stdasync-slow-compared-to-simple-detached-threads?noredirect=1&lq=1

https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3451.pdf

https://scottmeyers.blogspot.com/2013/03/stdfutures-from-stdasync-arent-special.html




