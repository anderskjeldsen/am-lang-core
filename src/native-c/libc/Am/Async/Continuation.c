#include <libc/core.h>
#include <Am/Async/Continuation.h>
#include <Am/Async/ContinuationCancelledException.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

#include <stdlib.h>
#include <stdatomic.h>

// Phase 1 of Am.Async.Continuation. The eventual goal is that a
// Continuation wraps a `suspend_state *` and `resume(value)` re-enters
// the suspended computation by writing the value into the state's
// result slot and calling `state->function(state)`. The
// suspend(cont) { ... } language construct that produces such a
// state-bound continuation is Phase 2 work.
//
// What Phase 1 gives us:
//   * A real Continuation aobject creatable from AmLang via `new
//     Continuation<T>()`.
//   * A done flag that's flipped atomically by the first resume()
//     call. Subsequent resume() calls are silent no-ops, so
//     "whoever-finishes-first-wins" races between completion sources
//     are safe by construction.
//   * Safe cross-thread invocation: resume() may be called from any
//     thread; the atomic compare-exchange guarantees exactly one
//     winner.
//   * A captured value that the (future) suspend block's resume label
//     reads back. Stored as `nullable_value` so it can hold a primitive
//     (int / long / object / ...) without boxing.
//
// What Phase 1 cannot yet do (Phase 2 picks it up):
//   * Actually re-enter a suspended computation. resume() sets `done`
//     and records the value, but there is no `suspend_state *` to
//     dispatch through. The language-level `suspend(cont) { ... }`
//     block in Phase 2 will hand-attach a state pointer to a freshly
//     created Continuation and resume() will then drive it.

typedef struct _Am_Async_Continuation_data Am_Async_Continuation_data;
struct _Am_Async_Continuation_data {
    // Set by the first resume() call. Subsequent resume() calls observe
    // it as true and return without doing anything. Using atomic_flag
    // would be slimmer but we also want to *read* the flag (isDone),
    // and atomic_flag is test-and-set only. atomic_bool with
    // compare-exchange + load is the right primitive.
    atomic_bool done;

    // Phase 2: pointer to the `suspend_state *` this continuation
    // resumes. NULL in Phase 1 — resume() just sets `done`; nothing
    // drives the parent chain because there isn't one yet.
    void *state;
};

function_result Am_Async_Continuation__native_init_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;

    Am_Async_Continuation_data *data = malloc(sizeof(Am_Async_Continuation_data));
    atomic_store_explicit(&data->done, false, memory_order_relaxed);
    data->state = NULL;
    this->object_properties.class_object_properties.object_data.value.custom_value = data;

__exit: ;
    return __result;
}

function_result Am_Async_Continuation__native_release_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;

    Am_Async_Continuation_data *data =
        (Am_Async_Continuation_data *) this->object_properties.class_object_properties.object_data.value.custom_value;
    if (data != NULL) {
        // Orphan-cont cancellation: if resume() was never called, the
        // suspended computation is owed an exception. We CAS `done` to
        // true to race with any concurrent resume() — exactly one of
        // us wins. The loser becomes a no-op; the winner takes
        // ownership of driving state->function with the cancellation
        // exception installed in state->result.
        bool expected = false;
        if (atomic_compare_exchange_strong_explicit(
                &data->done, &expected, true,
                memory_order_acq_rel, memory_order_acquire))
        {
            suspend_state *state_to_fire = (suspend_state *) data->state;
            if (state_to_fire != NULL && state_to_fire->function != NULL) {
                aobject *ex = __allocate_object(&Am_Async_ContinuationCancelledException);
                Am_Async_ContinuationCancelledException_f_ContinuationCancelledException_0(ex);
                Am_Async_ContinuationCancelledException___init_instance(
                    (nullable_value){ .value.object_value = ex });
                aobject *stit = __create_string_constant(
                    "Continuation dropped without resume",
                    &Am_Lang_String);
                __throw_exception(&state_to_fire->result, ex, stit);
                __decrease_reference_count(ex);
                __decrease_reference_count(stit);
                state_to_fire->function(state_to_fire);
            }
        }
        free(data);
        this->object_properties.class_object_properties.object_data.value.custom_value = NULL;
    }

__exit: ;
    return __result;
}

function_result Am_Async_Continuation__native_mark_children_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;
__exit: ;
    return __result;
}

// resume(): the only mutating entry point. Phase 1 was value-less; in
// Phase 2 the runtime side drives the parent suspend chain by firing
// `state->function(state)`. The compiler's `suspend(cont) { ... }`
// block attaches the state via `__attachState` before handing `cont`
// to user code.
function_result Am_Async_Continuation_resume_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;

    Am_Async_Continuation_data *data =
        (Am_Async_Continuation_data *) this->object_properties.class_object_properties.object_data.value.custom_value;

    suspend_state *state_to_fire = NULL;

    if (data != NULL) {
        // Atomic compare-exchange: only the first caller wins. Anyone
        // arriving after is a no-op (data->done already true, swap
        // fails, body skipped). This is the idempotence guarantee.
        bool expected = false;
        if (atomic_compare_exchange_strong_explicit(
                &data->done, &expected, true,
                memory_order_acq_rel, memory_order_acquire))
        {
            // We won the race. Capture the attached state pointer
            // before releasing our ref on `this` (so a parallel release
            // can't free the data underneath us). Then fire below.
            state_to_fire = (suspend_state *) data->state;
        }
    }

__exit: ;

    // Fire AFTER dropping our reference on `this`. The parent's resumed
    // code may itself release the continuation (drops to 0 → free()),
    // which is fine because we no longer touch `this` past here.
    if (state_to_fire != NULL && state_to_fire->function != NULL) {
        state_to_fire->function(state_to_fire);
    }

    return __result;
}

// Compiler-only helper. Called from the rendered C of a `suspend(cont)`
// block right after allocating the Continuation aobject: hands the
// freshly-saved `suspend_state *` to the continuation so the later
// `resume()` call can re-enter the parent.
//
// Not exposed to AmLang code — the leading `__` and the lack of an
// `.aml` declaration keeps it out of the AmLang namespace.
void Am_Async_Continuation___attachState(aobject * const this, suspend_state * const state)
{
    if (this == NULL) return;
    Am_Async_Continuation_data *data =
        (Am_Async_Continuation_data *) this->object_properties.class_object_properties.object_data.value.custom_value;
    if (data != NULL) {
        data->state = state;
    }
}

function_result Am_Async_Continuation_isDone_0(aobject * const this)
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    Am_Async_Continuation_data *data =
        (Am_Async_Continuation_data *) this->object_properties.class_object_properties.object_data.value.custom_value;

    bool done = false;
    if (data != NULL) {
        done = atomic_load_explicit(&data->done, memory_order_acquire);
    }
    __result.return_value.value.bool_value = done;
    __result.return_value.flags = 0;

__exit: ;
    return __result;
}
