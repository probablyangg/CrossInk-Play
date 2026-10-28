// CrossInk Play adapter tests. Upstream rules suites live beside this suite;
// these assertions exercise the two layout operations supplied for SDK 6993701.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/ShelfScreen.h"
#include "../../src/apps_local/ui/ToyboxIcons.h"

namespace fui = freeink::ui;
namespace {
int checks = 0;
int failures = 0;
void check(bool ok, const char* message, int line) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("FAIL line %d: %s\n", line, message);
  }
}
#define CHECK(value) check((value), #value, __LINE__)

class Target final : public fui::DrawTarget {
 public:
  struct Fill {
    fui::Rect rect;
    fui::Paint paint;
  };
  struct Text {
    fui::Rect rect;
    std::string value;
    fui::TextStyle style;
  };
  std::vector<Fill> fills;
  std::vector<Text> texts;
  fui::Size measureText(fui::FontId, const char* value, fui::TextStyle) const override {
    return fui::Size{static_cast<int16_t>(value ? std::strlen(value) * 8 : 0), 20};
  }
  int16_t lineHeight(fui::FontId) const override { return 20; }
  void fill(fui::Rect r, fui::Paint p, uint8_t = 0, uint8_t = 0xFF) override { fills.push_back({r, p}); }
  void stroke(fui::Rect, fui::Paint, uint8_t, uint8_t = 0, uint8_t = 0xFF) override {}
  void line(fui::Point, fui::Point, uint8_t, fui::Paint) override {}
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void text(fui::Rect r, const char* value, fui::TextStyle s) override { texts.push_back({r, value ? value : "", s}); }
  void bitmap(fui::Rect, fui::BitmapRef, fui::BitmapMode, fui::Paint = {},
              fui::Rotation = fui::Rotation::None) override {}
};

fui::DeviceContext device(bool landscape = false) {
  fui::DeviceContext context;
  context.width = landscape ? 800 : 480;
  context.height = landscape ? 480 : 800;
  context.safeArea = fui::Insets{10, 1, 0, 1};
  context.hasTouch = true;
  context.hasButtons = true;
  return context;
}

void chromeIsFullBleedAndContentClearsTheBezel(bool landscape) {
  Target target;
  toybox::Interactions interactions;
  fui::InputSnapshot input;
  // A temporary context deliberately exercises Frame's ownership guarantee.
  toybox::Frame frame(target, device(landscape), input, interactions);
  toybox::Screen screen(frame);
  screen.setContentMarginAbsolute(fui::Insets{7, 8, 9, 10});
  const auto inset = screen.body();
  CHECK(inset.x == 10 && inset.y == 7);
  CHECK(inset.width == device(landscape).width - 18);
  CHECK(inset.height == device(landscape).height - 16);

  fui::HeaderProps props;
  props.title = "PLAY";
  toybox::headerBand(screen, props);
  CHECK(screen.body().y == toybox::kChromeHeight);
  bool hasBand = false;
  for (const auto& fill : target.fills) {
    if (fill.rect.x == 0 && fill.rect.y == 0 && fill.rect.width == device(landscape).width &&
        fill.rect.height == toybox::kHeaderHeight && fill.paint.color == fui::Color::Black)
      hasBand = true;
  }
  CHECK(hasBand);
  bool titleIsVisible = false;
  for (const auto& text : target.texts) {
    if (text.value == "PLAY") {
      titleIsVisible = text.rect.y >= device(landscape).safeArea.top && text.style.color == fui::Color::White;
    }
  }
  CHECK(titleIsVisible);
  screen.insetContent(fui::Insets{toybox::kBodyGutter, toybox::kMargin, toybox::kMargin, toybox::kMargin});
  CHECK(screen.body().y == toybox::kBodyTop);
}

void explicitHeaderKeepsLayoutAndRoutesItsAction() {
  Target target;
  toybox::Interactions interactions;
  fui::InputSnapshot input;
  toybox::Frame frame(target, device(), input, interactions);
  toybox::Screen screen(frame);
  const auto before = screen.contentRect();
  fui::HeaderProps props;
  props.title = "PLAY";
  props.trailingLabel = "DONE";
  props.trailingAction = 42;
  screen.header(props, fui::Rect{0, 10, 480, 65});
  const auto after = screen.contentRect();
  CHECK(before.x == after.x && before.y == after.y && before.width == after.width && before.height == after.height);
  bool routesDone = false;
  for (size_t i = 0; i < interactions.count(); ++i) {
    const auto& hit = interactions.data()[i];
    if (hit.action != 42) continue;
    fui::InputSnapshot tap;
    tap.touchReleased = true;
    tap.touchX = static_cast<int16_t>(hit.rect.x + hit.rect.width / 2);
    tap.touchY = static_cast<int16_t>(hit.rect.y + hit.rect.height / 2);
    routesDone = interactions.route(tap).action == 42;
  }
  CHECK(routesDone);
}

void everyShelfRowIsReachable() {
  const auto context = device();
  const auto paging = shelfui::pagingFor(context, toybox::themeTokens(), true, 25);
  CHECK(paging.rowsPerPage > 0 && paging.pageCount > 1);
  int visited = 0;
  for (int page = 0; page < paging.pageCount; ++page) {
    Target target;
    toybox::Interactions interactions;
    fui::InputSnapshot input;
    toybox::Frame frame(target, context, input, interactions);
    toybox::Screen screen(frame);
    fui::ListItem rows[25] = {};
    const freeink::Icon* icons[25] = {};
    int count = 0;
    for (int index = page * paging.rowsPerPage; index < 25 && count < paging.rowsPerPage; ++index, ++count) {
      rows[count].label = "CHESS";
      rows[count].actionValue = static_cast<int16_t>(index);
      icons[count] = &icon_chess_32;
    }
    shelfui::MenuModel model;
    model.title = "PLAY";
    model.mark = &icon_games_32;
    model.icons = icons;
    model.items = rows;
    model.count = count;
    model.page = page;
    model.pageCount = paging.pageCount;
    model.playerName = "A Calm Fox";
    shelfui::buildMenu(screen, model);
    CHECK(!interactions.overflowed());
    for (size_t i = 0; i < interactions.count(); ++i) {
      const auto& hit = interactions.data()[i];
      if (hit.action != shelfui::ActionOpen) continue;
      fui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = static_cast<int16_t>(hit.rect.x + hit.rect.width / 2);
      tap.touchY = static_cast<int16_t>(hit.rect.y + hit.rect.height / 2);
      const auto action = interactions.route(tap);
      CHECK(action.action == shelfui::ActionOpen && action.value == hit.value);
      CHECK(hit.value == visited);
      ++visited;
    }
  }
  CHECK(visited == 25);
}
}  // namespace

int main() {
  chromeIsFullBleedAndContentClearsTheBezel(false);
  chromeIsFullBleedAndContentClearsTheBezel(true);
  explicitHeaderKeepsLayoutAndRoutesItsAction();
  everyShelfRowIsReachable();
  std::printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
