// A wrapper's life in the record's filter slot, as the engine sees it.
//
// A list with no engine filter is still wrapped and judged by its sets alone. A slot that already holds
// a wrapper is left with it. A wrapper the engine destroys through its own vtable as it rebuilds the list
// is forgotten, and the rebuilt filter is wrapped afresh.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(list_without_an_engine_filter) {
    bed::Fixture f = bed::start(folder);
    void* bare = bed::make_record(1, "Bare", nullptr);
    bed::raise("on_game_loaded");
    bed::send("2|sell:10:terran|buy:00:");

    void* wrapper = bed::filter_slot(bare);
    bed::check("a list with no engine filter is wrapped", wrapper != nullptr);
    if (!wrapper) return;
    bed::engine().engine_filter_allows = false;
    bed::check("it allows what its sets allow", bed::wrapper_allows(wrapper, bed::make_partner(f.argon)));
    bed::check("it refuses what its sets refuse",
               !bed::wrapper_allows(wrapper, bed::make_partner(f.terran)));
}

BED_SCENARIO(slot_already_holding_a_wrapper) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");
    void* first_wrapper = bed::filter_slot(f.record);

    void* second = bed::make_record(1, "Shared", first_wrapper);
    bed::send("2|sell:00:|buy:00:");
    bed::check("a list whose slot already holds a wrapper is not wrapped again",
               bed::filter_slot(second) == first_wrapper);
    bed::check("that slot judges by the wrapper it holds",
               !bed::wrapper_allows(bed::filter_slot(second), bed::make_partner(f.terran)));
}

BED_SCENARIO(engine_destroys_the_wrapper) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");

    void** wrapper_vtable = bed::at<void**>(bed::filter_slot(f.record), 0);

    // The rebuild destroys the wrapper through its vtable, then builds the list's filter afresh.
    bed::check("the rebuild after a destroy runs without a fault", bed::build_safely(f.record));
    void* fresh = bed::filter_slot(f.record);
    bed::check("the rebuilt filter is wrapped afresh",
               fresh != nullptr && bed::at<void**>(fresh, 0) == wrapper_vtable);
    bed::check("the fresh wrapper judges by the list's rule",
               !bed::wrapper_allows(fresh, bed::make_partner(f.terran)) &&
                   bed::wrapper_allows(fresh, bed::make_partner(f.argon)));
}
