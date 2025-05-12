#include "image_viewer.h"

#include "config.h"
#include "def.h"
#include "resourceManager.h"
#include "screen.h"
#include "sdlutils.h"
#include "text_viewer.h"

#define VIEWER_MARGIN 1

ImageViewer::ImageViewer(CPanel *panel)
    : panel_(panel)
{
    init();
    setPath(panel->getHighlightedItemFull());
}

void ImageViewer::init()
{
    // Create background image
    background_ = SDLSurfaceUniquePtr { SDL_utils::createImage(screen.actual_w,
        screen.actual_h, SDL_MapRGB(screen.surface->format, COLOR_BG_1)) };

    // Transparency grid background.
    constexpr int kTransparentBgRectSize = 10;
    const Uint32 colors[2] = {
        SDL_MapRGB(background_->format, 240, 240, 240),
        SDL_MapRGB(background_->format, 155, 155, 155),
    };
    int j = 0;
    const int rect_w = static_cast<int>(kTransparentBgRectSize * screen.ppu_x);
    const int rect_h = static_cast<int>(kTransparentBgRectSize * screen.ppu_y);
    for (int j = 0, y = Y_LIST * screen.ppu_y; y < screen.actual_h;
         y += rect_h, ++j) {
        for (int i = 0, x = 0; x < screen.actual_w; x += rect_w, ++i) {
            SDL_Rect rect = SDL_utils::makeRect(x, y, rect_w, rect_h);
            SDL_FillRect(background_.get(), &rect, colors[(i + j) % 2]);
        }
    }
}

void ImageViewer::setPath(std::string &&path)
{
    filename_ = std::move(path);
    image_ = nullptr; // Release memory first
    image_ = SDLSurfaceUniquePtr { SDL_utils::loadImageToFit(
        filename_, screen.w, screen.h - Y_LIST) };
    ok_ = (image_ != nullptr);
    if (!ok_) return;

    const auto &fonts = CResourceManager::instance().getFonts();

    // Print title
    {
        SDL_Rect rect = SDL_utils::Rect(
            0, 0, screen.actual_w, HEADER_H * screen.ppu_y);
        SDL_FillRect(background_.get(), &rect,
            SDL_MapRGB(background_->format, COLOR_BORDER));
    }
    {
        SDLSurfaceUniquePtr tmp { SDL_utils::renderText(
            fonts, filename_, Globals::g_colorTextTitle, { COLOR_TITLE_BG }) };
        if (tmp->w > background_->w - 2 * VIEWER_MARGIN) {
            SDL_Rect rect;
            rect.x = tmp->w - (background_->w - 2 * VIEWER_MARGIN);
            rect.y = 0;
            rect.w = background_->w - 2 * VIEWER_MARGIN;
            rect.h = tmp->h;
            SDL_utils::applyPpuScaledSurface(VIEWER_MARGIN * screen.ppu_x,
                HEADER_PADDING_TOP * screen.ppu_y, tmp.get(), background_.get(),
                &rect);
        } else {
            SDL_utils::applyPpuScaledSurface(VIEWER_MARGIN * screen.ppu_x,
                HEADER_PADDING_TOP * screen.ppu_y, tmp.get(),
                background_.get());
        }
    }
}

void ImageViewer::onResize()
{
    image_ = nullptr;
    background_ = nullptr;
    init();
    setPath(std::move(filename_));
}

void ImageViewer::render(const bool focused) const
{
    SDL_utils::applyPpuScaledSurface(0, 0, background_.get(), screen.surface);
    SDL_utils::applyPpuScaledSurface((screen.actual_w - image_->w) / 2,
        Y_LIST * screen.ppu_y
            + (screen.actual_h - Y_LIST * screen.ppu_y - image_->h) / 2,
        image_.get(), screen.surface);
}

bool ImageViewer::nextOrPreviousImage(int direction)
{
    while (true) {
        if (direction == -1) {
            if (!panel_->moveCursorUp(1)) return false;
        } else {
            if (!panel_->moveCursorDown(1)) return false;
        }
        std::string new_path = panel_->getHighlightedItemFull();

        constexpr std::size_t kMaxFileSize = 16777216; // = 16 MB
        if (File_utils::getFileSize(new_path) > kMaxFileSize) continue;

        setPath(std::move(new_path));
        if (ok()) return true;
    }
}

// Key press management
bool ImageViewer::keyPress(
    const SDL_Event &event, SDLC_Keycode key, ControllerButton button)
{
    CWindow::keyPress(event, key, button);
    const auto &c = config();
    const auto sym = event.key.keysym.sym;
    if (key == c.key_system || button == c.gamepad_system || key == c.key_parent
        || button == c.gamepad_parent) {
        m_retVal = -1;
        return true;
    }

    if (key == c.key_left || button == c.gamepad_left || key == c.key_up
        || button == c.gamepad_up) {
        return nextOrPreviousImage(-1);
    }
    if (key == c.key_right || button == c.gamepad_right || key == c.key_down
        || button == c.gamepad_down) {
        return nextOrPreviousImage(1);
    }

    return false;
}

bool ImageViewer::mouseWheel(int dx, int dy)
{
    CWindow::mouseWheel(dx, dy);
    if (dy < 0) return nextOrPreviousImage(-1);
    if (dy > 0) return nextOrPreviousImage(1);
    return false;
}
