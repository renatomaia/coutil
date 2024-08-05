# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Fix to avoid suspend task not awaiting channel.
- Fix to avoid overflow of Lua stack after many resumptions.
- Fix to avoid corruption of Lua stack of suspended tasks.
- Fix to release the resources of a thread pool.
- Fix to copy values between states safely.
- Fix to handle errors while transferring values between tasks.
- Fix to avoid collection of channel while syncing.
- Fix to avoid collection of resumed thread.
- Fix to correctly get a resumed coroutine error.
- Fix to ensure space for returns of file:info.

## [2.0.0] - 2021-07-19

### Added

- Support for cooperative scheduling based on system events.
- Support to check whether system events are being processed.
- Support to stop processing system events.
- Support for coroutines that execute in their own thread.
- Support for preemptive thread pools
- Support for message channels for threads.
- Support to get the time of the system.
- Support to measure the time.
- Support to await for a period of time.
- Support to suspend the entire thread.
- Support to get names for network addresses.
- Support to resolve network names (DNS).
- Support for UDP sockets.
- Support for UDP multicast options.
- Support for TCP sockets.
- Support for local sockets (pipes).
- Support to access terminal (TTY) streams.
- Support to await for the process to receive signals.
- Support to emit signals to processes.
- Support to execute processes.
- Support to create pipe to executed processes.
- Support to redirect STDIO of executed processes.
- Support to access redirected STDIO.
- Support to create packed environment variables for executed processes.
- Support to manipulate the current process environment variables.
- Support to manipulate the current process priority.
- Support to manipulate the current directory.
- Support to read and write files.
- Support to synchronize opened files.
- Support for direct file to file transfers.
- Support to copy files.
- Support to move files.
- Support to remove files.
- Support to change file's permission.
- Support to change file ownership.
- Support to change file timestamps.
- Support to create file links.
- Support to create directories.
- Support to list directories.
- Support for temporary files and directories.
- Support to get file information.
- Support to get current process information.
- Support to get operating system information.
- Support to get hardware usage information.
- Support for secure pseudorandom number generator.

### Changed

- Await functions are interrupted by resuming the caller.

### Removed

- `event.awaiteach` and `queued.awaiteach` were removed.

## [1.1.0] - 2018-11-11

### Added

- Support for queued events.
- Support for [mutexes](https://en.wikipedia.org/wiki/Mutex).

## [1.0.0] - 2018-07-13

### Added

- Support for coroutine finalizers.
- Syncronization abstractions for [cooperative multitasking](https://en.wikipedia.org/wiki/Cooperative_multitasking) using coroutines: [events](https://en.wikipedia.org/wiki/Async/await) and [promises](https://en.wikipedia.org/wiki/Futures_and_promises).


[unreleased]: https://github.com/renatomaia/coutil/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/renatomaia/coutil/compare/v1.1.0...v2.0.0
[1.1.0]: https://github.com/renatomaia/coutil/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/renatomaia/coutil/tree/v1.0.0