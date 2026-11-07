// Deleting lists leaves every other list judged by its own rule.
//
// Four lists each refuse their own faction on a sale. The game deletes a list as RemoveBlacklist does:
// it destroys the list's filter through vtable slot 0 and the id stops resolving. The first, a middle and
// the last list are deleted in turn, each followed by the report the editor asks for. After every
// deletion, and after a reload, each surviving list refuses its own faction and only that one, and the
// reported table no longer carries the deleted list.

#include "common.h"
#include "events.h"
#include "scenario.h"

#include <string>
#include <vector>

namespace {

struct List {
    int id;
    void* record;
    void* faction;
    std::string entry;
    bool deleted = false;
};


// The game's RemoveBlacklist.
void delete_list(List& list) {
    bed::forget_record(list.record);
    list.deleted = true;
}

// Every surviving list refuses its own faction and allows every other list's.
bool each_list_judges_by_its_own_rule(const std::vector<List>& lists) {
    for (const List& judged : lists) {
        if (judged.deleted) continue;
        void* wrapper = bed::filter_slot(judged.record);
        for (const List& other : lists) {
            const bool allowed = bed::wrapper_allows(wrapper, bed::make_partner(other.faction));
            if (allowed == (&other == &judged)) return false;
        }
    }
    return true;
}

bool report_leaves_out_deleted(const std::vector<List>& lists) {
    const std::string report = bed::notation(bed::last_lua(bed::events::sets));
    for (const List& list : lists) {
        const bool carried = report.find(std::to_string(list.id) + "|") != std::string::npos;
        if (carried == list.deleted) return false;
    }
    return true;
}

}  // namespace

BED_SCENARIO(list_deletion) {
    bed::Fixture f = bed::start(folder);
    std::vector<List> lists = {
        {1, f.record, f.terran, "1|sell:10:terran|buy:00:"},
        {2, bed::make_record(1, "Second", bed::make_engine_filter()), f.argon, "2|sell:10:argon|buy:00:"},
        {3, bed::make_record(1, "Third", bed::make_engine_filter()), bed::add_faction("boron"),
         "3|sell:10:boron|buy:00:"},
        {4, bed::make_record(1, "Fourth", bed::make_engine_filter()), bed::add_faction("paranid"),
         "4|sell:10:paranid|buy:00:"},
    };
    bed::raise("on_game_loaded");
    for (const List& list : lists) bed::send(list.entry.c_str());
    bed::check("every list starts judged by its own rule", each_list_judges_by_its_own_rule(lists));

    for (int position : {0, 2, 3}) {
        delete_list(lists[static_cast<std::size_t>(position)]);
        bed::raise(bed::events::request);
        const std::string which = position == 0 ? "the first" : position == 3 ? "the last" : "a middle";
        bed::check(("after deleting " + which + " list, every other list is judged by its own rule").c_str(),
                   each_list_judges_by_its_own_rule(lists));
        bed::check(("after deleting " + which + " list, the reported table leaves it out").c_str(),
                   report_leaves_out_deleted(lists));
    }

    bed::raise("on_game_loaded");
    for (const List& list : lists) {
        if (!list.deleted) bed::send(list.entry.c_str());
    }
    bed::check("after a reload the surviving list is judged by its own rule",
               each_list_judges_by_its_own_rule(lists));
}
