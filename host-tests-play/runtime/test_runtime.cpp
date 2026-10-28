#include <cstdio>
#include <thread>

#include "PaintClock.h"
#include "RevealedInteractions.h"

namespace fui = freeink::ui;
using Table = paintclock::RevealedInteractions<4>;

namespace {
unsigned checks = 0;
unsigned failures = 0;

#define CHECK(expression)                                                   \
  do {                                                                      \
    ++checks;                                                               \
    if (!(expression)) {                                                    \
      ++failures;                                                           \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
    }                                                                       \
  } while (false)

struct FreshClock {
  FreshClock() { paintclock::counter().store(0); }
};

fui::Interaction button(fui::ActionId action, fui::State state = fui::StateNormal) {
  return {{10, 10, 100, 50}, action, 0, fui::InputDefault, state, 0};
}

fui::InputSnapshot tap() {
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = 40;
  input.touchY = 30;
  return input;
}

void build(Table& table, const fui::Interaction& interaction, bool publish = false) {
  if (publish) table.beginPublishCycle();
  table.beginBuild();
  table.clear();
  CHECK(static_cast<Table::Buffer&>(table).addInteraction(interaction));
  if (publish) table.publish();
}

void completedPaintsOnly() {
  FreshClock clock;
  paintclock::PaintCompletion paint;
  paintclock::RevealGate gate;
  gate.arm();
  gate.markBuilt();
  paint.waited();
  CHECK(paintclock::painted() == 0);
  CHECK(!gate.revealed());
  paint.startedAsync();
  CHECK(paintclock::painted() == 0);
  CHECK(!gate.revealed());
  paint.waited();
  CHECK(paintclock::painted() == 1);
  CHECK(gate.revealed());

  // A second wait cannot reveal a new screen: no new waveform was started.
  gate.arm();
  gate.markBuilt();
  paint.waited();
  CHECK(paintclock::painted() == 1);
  CHECK(!gate.revealed());

  // A blocking paint supersedes pending work without a phantom later paint.
  paint.startedAsync();
  paint.completed();
  CHECK(paintclock::painted() == 2);
  CHECK(gate.revealed());
  gate.arm();
  gate.markBuilt();
  paint.waited();
  CHECK(paintclock::painted() == 2);
  CHECK(!gate.revealed());
}

void revealUsesLatestBuildAndLatches() {
  FreshClock clock;
  paintclock::RevealGate gate;
  CHECK(gate.revealed());
  gate.arm();
  gate.markBuilt();
  CHECK(!gate.revealed());
  paintclock::notePainted();
  gate.markBuilt();
  CHECK(!gate.revealed());
  paintclock::notePainted();
  CHECK(gate.revealed());
  gate.markBuilt();
  CHECK(gate.revealed());
}

void geometryWaitsWhenMeaningChanges() {
  FreshClock clock;
  paintclock::SurfaceGate gate;
  const auto dig = paintclock::mixMeaning(paintclock::kMeaningSeed, 0);
  const auto flag = paintclock::mixMeaning(paintclock::kMeaningSeed, 1);
  gate.noteBuilt(dig);
  CHECK(gate.routable(dig));  // No physical frame yet, as in host builders.
  paintclock::notePainted();
  CHECK(gate.routable(dig));
  gate.noteBuilt(flag);
  CHECK(!gate.routable(flag));
  CHECK(gate.routable(dig));
  gate.noteBuilt(flag);
  CHECK(!gate.routable(flag));  // An unpainted rebuild never becomes shown.
  paintclock::notePainted();
  CHECK(gate.routable(flag));
  gate.noteBuilt(flag);
  CHECK(gate.routable(flag));  // A highlight/score repaint keeps the meaning.

  paintclock::SurfaceGate defaultActivity;
  defaultActivity.noteBuilt(0);
  CHECK(defaultActivity.routable(0));  // Reader activities do not opt in.
}

void restoredGameWaitsForItsOwnFrame() {
  FreshClock clock;
  paintclock::SurfaceGate gate;
  gate.noteBuilt(42);
  paintclock::notePainted();
  CHECK(gate.routable(42));

  // A frontlight overlay paints above the game, then is popped. The old
  // game table has the same meaning, but its pixels are not visible yet.
  paintclock::notePainted();
  gate.invalidate();
  CHECK(!gate.revealed());
  CHECK(!gate.routable(42));
  paintclock::notePainted();  // Another overlay paint cannot open the game.
  CHECK(!gate.revealed());
  CHECK(!gate.routable(42));
  gate.noteBuilt(42);
  CHECK(!gate.revealed());
  CHECK(!gate.routable(42));
  paintclock::notePainted();
  CHECK(gate.revealed());
  CHECK(gate.routable(42));
}

void changedActionWaitsForInk() {
  FreshClock clock;
  Table table;
  build(table, button(1));
  CHECK(table.route(tap()).action == 1);
  paintclock::notePainted();
  build(table, button(2));
  CHECK(!table.routable());
  CHECK(table.route(tap()).action == fui::NO_ACTION);
  build(table, button(2));
  CHECK(!table.routable());
  CHECK(table.route(tap()).action == fui::NO_ACTION);
  paintclock::notePainted();
  CHECK(table.routable());
  CHECK(table.route(tap()).action == 2);
}

void cosmeticFeedbackKeepsTheRelease() {
  FreshClock clock;
  Table table;
  build(table, button(1));
  paintclock::notePainted();
  auto down = tap();
  down.touchReleased = false;
  down.touchPressed = true;
  table.route(down);
  CHECK(table.activeIndex() == 0);
  build(table, button(1, fui::StateActive));
  CHECK(table.routable());
  CHECK(table.route(tap()).action == 1);
  build(table, button(1, fui::StateFocused));
  CHECK(table.routable());
  CHECK(table.route(tap()).action == 1);
}

void disabledAndMovedControlsWait() {
  FreshClock clock;
  Table table;
  build(table, button(1, fui::StateDisabled));
  paintclock::notePainted();
  CHECK(table.route(tap()).action == fui::NO_ACTION);
  build(table, button(1));
  CHECK(!table.routable());
  CHECK(table.route(tap()).action == fui::NO_ACTION);
  paintclock::notePainted();
  CHECK(table.route(tap()).action == 1);
  auto moved = button(1);
  moved.rect.x = 25;
  build(table, moved);
  CHECK(!table.routable());
  paintclock::notePainted();
  CHECK(table.route(tap()).action == 1);
}

void publishedGenerationIsTheShownOne() {
  FreshClock clock;
  Table table;
  build(table, button(1), true);
  paintclock::notePainted();
  CHECK(table.routePublished(tap()).action == 1);
  build(table, button(2), true);
  CHECK(!table.publishedRoutable());
  CHECK(table.routePublished(tap()).action == fui::NO_ACTION);
  paintclock::notePainted();
  CHECK(table.routePublished(tap()).action == 2);

  // The building slot now contains 1 from two generations ago. Only the
  // published slot (2) represents what the panel shows before this rebuild.
  build(table, button(1), true);
  CHECK(!table.publishedRoutable());
  CHECK(table.routePublished(tap()).action == fui::NO_ACTION);
  build(table, button(1), true);
  CHECK(!table.publishedRoutable());
  paintclock::notePainted();
  CHECK(table.routePublished(tap()).action == 1);
  build(table, button(1), true);
  CHECK(table.publishedRoutable());
  CHECK(table.routePublished(tap()).action == 1);
}

void paintCounterIsSharedAcrossTasks() {
  FreshClock clock;
  const auto paint = [] {
    for (unsigned i = 0; i < 10000; ++i) paintclock::notePainted();
  };
  std::thread first(paint);
  std::thread second(paint);
  first.join();
  second.join();
  CHECK(paintclock::painted() == 20000);
}
}  // namespace

int main() {
  completedPaintsOnly();
  revealUsesLatestBuildAndLatches();
  geometryWaitsWhenMeaningChanges();
  restoredGameWaitsForItsOwnFrame();
  changedActionWaitsForInk();
  cosmeticFeedbackKeepsTheRelease();
  disabledAndMovedControlsWait();
  publishedGenerationIsTheShownOne();
  paintCounterIsSharedAcrossTasks();
  std::printf("Play runtime: %u checks, %u failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
