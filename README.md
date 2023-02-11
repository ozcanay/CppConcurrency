```fut``` will represent ```std::future``` values.

I will not use auto to underline the return types.

# Composable Concurrency

Composable concurrency refers to a programming model for designing and building concurrent systems that allows for the creation of individual, isolated units of concurrent execution, known as "tasks", which can be composed and coordinated in a way that ensures reliable, deterministic behavior. The idea behind composable concurrency is to provide a high-level abstraction for dealing with concurrency that is easy to understand and use, while also allowing for fine-grained control over the behavior of individual tasks.

In a composable concurrency model, tasks are usually implemented as single-threaded, event-driven entities that communicate with each other through channels or other means. This allows for tasks to be written and tested in isolation, and then composed together to form larger systems, while ensuring that each task's behavior remains predictable and deterministic. The coordination of tasks is handled by a scheduler, which ensures that tasks are executed in a manner that meets the specified constraints and guarantees, such as ordering, priority, and mutual exclusion.

Composable concurrency is used in a variety of applications, including operating systems, distributed systems, and real-time systems, where the need for reliable, predictable behavior and efficient coordination of multiple concurrent entities is important.