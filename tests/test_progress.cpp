#include "test.hpp"

#include <progress.hpp>

using namespace kid;

TEST(progress_keeps_the_best_stars_and_the_highest_score) {
    Progress p;
    CHECK(p.best("A").stars == 0);
    CHECK(p.record("A", 1, 20000));
    CHECK(p.record("A", 3, 30000));
    CHECK(!p.record("A", 2, 25000));  // worse on both counts: nothing changes
    CHECK(p.best("A").stars == 3);
    CHECK(p.best("A").score == 30000);
    CHECK(p.record("A", 1, 40000));  // a better score with fewer stars keeps the stars
    CHECK(p.best("A").stars == 3 && p.best("A").score == 40000);
    CHECK(!p.record("B", 0, 10000));  // not a win
    CHECK(p.best("B").stars == 0);
    CHECK(p.record("B", 2, 15000));
    CHECK(p.total_stars() == 5);
}

TEST(progress_round_trips_through_its_text_form) {
    Progress p;
    p.record("First Pig", 3, 25000);
    p.record("Bunker", 2, 31500);
    p.record("A name with spaces", 1, 5000);
    const Progress q = parse_progress(format_progress(p));
    CHECK(q.levels.size() == 3);
    CHECK(q.best("First Pig").stars == 3 && q.best("First Pig").score == 25000);
    CHECK(q.best("Bunker").stars == 2 && q.best("Bunker").score == 31500);
    CHECK(q.best("A name with spaces").stars == 1 && q.best("A name with spaces").score == 5000);
}

TEST(progress_skips_lines_that_do_not_parse) {
    const Progress p = parse_progress("3 25000 First Pig\ngarbage\n9 1 Too Many Stars\n2 -5 Negative\n2 2\n\n1 4000 Bunker");
    CHECK(p.levels.size() == 2);
    CHECK(p.best("First Pig").stars == 3);
    CHECK(p.best("Bunker").score == 4000);
}

TEST(levels_unlock_one_at_a_time) {
    const std::vector<std::string> order = {"One", "Two", "Three"};
    Progress p;
    CHECK(unlocked(p, order, 0));
    CHECK(!unlocked(p, order, 1));
    CHECK(!unlocked(p, order, 2));
    CHECK(!unlocked(p, order, -1) && !unlocked(p, order, 3));
    CHECK(next_to_play(p, order) == 0);
    p.record("One", 1, 5000);  // any win opens the next level, one star is enough
    CHECK(unlocked(p, order, 1));
    CHECK(!unlocked(p, order, 2));
    CHECK(next_to_play(p, order) == 1);
    p.record("Three", 3, 9000);  // a later level's result (an old save) does not open the gap before it
    CHECK(!unlocked(p, order, 2));
    CHECK(next_to_play(p, order) == 1);
    p.record("Two", 2, 7000);
    CHECK(unlocked(p, order, 2));
    CHECK(next_to_play(p, order) == 2);  // everything solved: offer the last level
}

TEST(unlocks_survive_saving_and_loading) {
    const std::vector<std::string> order = {"One", "Two"};
    Progress p;
    p.record("One", 2, 5000);
    const Progress q = parse_progress(format_progress(p));
    CHECK(unlocked(q, order, 1));
}
