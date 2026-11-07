// An allow-only set admits the partners it names and refuses the rest.
//
// The list's selling restriction is whitelisted to terran and argon, and a third faction exists.
// A sale to either named faction goes through and a sale to the third is refused, so the whitelist
// is read as a set rather than as a single value.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(allow_only) {
    bed::Fixture f = bed::start(folder);

    void* pioneer = bed::add_faction("pioneer");
    void* allow_engine = bed::make_engine_filter();
    void* allow_record = bed::make_record(1, "Allow List", allow_engine);

    bed::raise("on_game_loaded");
    const std::string entry =
        std::to_string(bed::record_count()) + "|sell:11:terran,argon|buy:00:";
    bed::send(entry.c_str());

    void* allow = bed::filter_slot(allow_record);
    bed::check("an allow-only set is wrapped", allow != allow_engine);

    bed::check("an allow-only set admits a partner it names",
               bed::sale_allows(allow, bed::make_partner(f.terran)) == true);
    bed::check("an allow-only set admits the second partner it names",
               bed::sale_allows(allow, bed::make_partner(f.argon)) == true);
    bed::check("an allow-only set refuses a partner it does not name",
               bed::sale_allows(allow, bed::make_partner(pioneer)) == false);
}
