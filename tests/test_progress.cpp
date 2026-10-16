#include "test.hpp"

#include <progress.hpp>

using namespace kid;

TEST(progress_keeps_the_best_stars_and_the_fewest_shots) {
    Progress p;
    CHECK(p.best("A").stars == 0);
    CHECK(p.record("A", 1, 3));
    CHECK(p.record("A", 3, 1));
    CHECK(!p.record("A", 2, 2));  // worse on both counts: nothing changes
    CHECK(p.best("A").stars == 3);
    CHECK(p.best("A").shots == 1);
    CHECK(!p.record("B", 0, 1));  // not a win
    CHECK(p.best("B").stars == 0);
    CHECK(p.record("B", 2, 2));
    CHECK(p.total_stars() == 5);
}

TEST(progress_round_trips_through_its_text_form) {
    Progress p;
    p.record("First Shot", 3, 1);
    p.record("Domino Run", 2, 2);
    p.record("A name with spaces", 1, 3);
    const Progress q = parse_progress(format_progress(p));
    CHECK(q.levels.size() == 3);
    CHECK(q.best("First Shot").stars == 3 && q.best("First Shot").shots == 1);
    CHECK(q.best("Domino Run").stars == 2 && q.best("Domino Run").shots == 2);
    CHECK(q.best("A name with spaces").stars == 1 && q.best("A name with spaces").shots == 3);
}

TEST(progress_skips_lines_that_do_not_parse) {
    const Progress p = parse_progress("3 1 First Shot\ngarbage\n9 1 Too Many Stars\n2 0 No Shots\n2 2\n\n1 4 Ice Shelf");
    CHECK(p.levels.size() == 2);
    CHECK(p.best("First Shot").stars == 3);
    CHECK(p.best("Ice Shelf").shots == 4);
}
