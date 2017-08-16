Index
=====

- [`coutil.event`](#synchronization-with-events)
- [`coutil.event.await`](#coutileventawait-e)
- [`coutil.event.awaitall`](#coutileventawaitall-e1-)
- [`coutil.event.awaitany`](#coutileventawaitany-e1-)
- [`coutil.event.awaiteach`](#coutileventawaiteach-f-e1-)
- [`coutil.event.emit`](#coutileventemit-e-)

- [`coutil.promise`](#asynchronous-requests-with-promises)
- [`coutil.promise.awaitall`](#coutilpromiseawaitall-)
- [`coutil.promise.awaitany`](#coutilpromiseawaitany-)
- [`coutil.promise.create`](#coutilpromisecreate)
- [`coutil.promise.onlypending`](#coutilpromiseonlypending-)
- [`coutil.promise.pickready`](#coutilpromisepickready-)

Contents
========

Events
------

This library provides functions for synchronization of coroutines using events.
Events can be emitted on any value that can be stored in a table as a key (all values except `nil`).
Coroutines might suspend awaiting for events on values, so they are resumed when events are emitted on these values.
The behavior is undefined if a coroutine suspended awaiting events is resumed by other means than the emission of the expected events.

### `coutil.event.await (e)`

Suspends the execution of the calling coroutine awaiting an event on value `e`.
It returns all the additional arguments passed to [`emit`](#coutileventemit-e-)).

### `coutil.event.awaitall ([e1, ...])`

Suspends the execution of the calling coroutine awaiting an event on all values `e1, ...`, and returns only after all these events are emitted.
Any `nil` in `e1, ...` is ignored.
Any repeated values in `e1, ...` are treated as a single one.
If `e1, ...` are not provided or are all `nil`, this function has no effect.

### `coutil.event.awaitany (e1, ...)`

Suspends the execution of the calling coroutine awaiting an event on any of the values `e1, ...`.
The value on which the event is emitted is returned, followed by any additional values passed to [`emit`](#coutileventemit-e-)).
Any `nil` in `e1, ...` is ignored.

### `coutil.event.awaiteach (f, [e1, ...])`

Suspends the execution of the calling coroutine awaiting an event on each of the values `e1, ...`.
Whenever one of these events are emited `f` is called with the value on which the event is emitted, followed by any additional values passed to [`emit`](#coutileventemit-e-)).

If `f` returns any value (including `nil`) then the suspension for the events is cancelled, and this function returns the values returned by `f`.
If `f` raises an error then the suspension for the events is cancelled, and this function raises the error.
Otherwise, this function return no values.

`f` is always executed in context of the calling coroutine, and the behavior is undefined if `f` suspends the calling coroutine (even to await events).
However `f` might resume another coroutine to await events.

Any `nil` in `e1, ...` is ignored.
Any repeated values in `e1, ...` are treated as a single one.
If `e1, ...` are not provided or are all `nil`, this function returns no value and has no effect.

### `coutil.event.emit (e, ...)`

Resumes all coroutines waiting for event `e` in the same order they were suspended.
The additional arguments `...` are passed to every resumed coroutine.
This function returns after resuming all coroutines awaiting event `e` at the moment this call.
It returns `true` if there was some coroutine awaiting the event, or `false` otherwise.

Promises
--------

This library provides functions for synchronization of coroutines using promises.
Promises are used to obtain results that will only be available at a later moment, when the promise is fulfilled.
Coroutines that claim an unfulfilled promise suspend awaiting its fulfillment in order to receive the results.
But once a promise is fulfilled its results become readily available for those that claims them.

### `coutil.promise.awaitall ([p, ...])`

Suspends the calling coroutine awaiting the fulfillment of all promises `p, ...`.
If all promises `p, ...` are fulfilled then this function has no effect.

### `coutil.promise.awaitany (p, ...)`

Returns a fulfilled promise from promises `p, ...`.
If there are no fulfilled promises in `p, ...`, it suspends the calling coroutine awaiting the fulfillment of any of the promises in `p, ...`.
If all promises in `p, ...` are fulfilled then this function has no effect.

### `coutil.promise.create ()`

Returns a new promise, followed by a fulfillment function.

The promise is a function that returns the promise's results.
If the promise is unfulfilled, it suspends the calling coroutine awaiting its fulfillment.
If the promise is called with an argument that evaluates to `true`, it never suspends the calling coroutine, and just returns `true` if the promise is fulfiled, or `false` otherwise.

The fulfillment function shall be called in order to fulfill the promise.
The arguments passed to the fulfillment function become the promise's results.
It returns `true` the first time it is called, or `false` otherwise.

Coroutines suspeded awaiting a promise's fulfillment are actually suspended awaiting an event on the promise (see [`await`](#coutileventawait-e)).
Such coroutines can be resumed by emitting an event on the promise.
In such case, the additional values to [`emit`](#coutileventemit-e-) will be returned by the promise.
But the promise will remain unfulfilled.
Therefore, when the promise is called again, it will suspend the calling coroutine awaiting an event on the promise (the promise fulfillment).

The first time the fulfillment function is called, an event is emitted on the promise with the promise's results as additional values.
Therefore, to suspend awaiting the fulfillment of a promise a coroutine may await an event on the promise.
However, once a promise is fulfilled, any coroutine that suspends awaiting events on the promise will remain suspended until an event is on the promise is emitted with function [`emit`](#coutileventemit-e-).

### `coutil.promise.onlypending ([p, ...])`

Returns all promises `p, ...` that are unfulfilled.

### `coutil.promise.pickready ([p, ...])`

Returns the first promise `p, ...` that is fulfilled, or no value if none of promises `p, ...` is fulfilled.
