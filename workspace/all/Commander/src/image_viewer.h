#ifndef IMAGE_VIEWER_H_
#define IMAGE_VIEWER_H_

#include <cstddef>
#include <string>

#include "panel.h"
#include "sdl_ptrs.h"
#include "window.h"

class ImageViewer : public CWindow {
  public:
    explicit ImageViewer(CPanel *panel);
    virtual ~ImageViewer() = default;

    ImageViewer(const ImageViewer &) = delete;
    ImageViewer &operator=(const ImageViewer &) = delete;

    bool ok() const { return ok_; }

  private:
    void setPath(std::string &&path);
    void init();
    void render(const bool focused) const override;
    bool keyPress(const SDL_Event &event, SDLC_Keycode key,
        ControllerButton button) override;
    bool mouseWheel(int dx, int dy) override;
    void onResize() override;
    bool isFullScreen() const override { return true; }

    bool nextOrPreviousImage(int direction);

    SDLSurfaceUniquePtr background_;
    SDLSurfaceUniquePtr image_;
    bool ok_;
    CPanel *panel_;
    std::string filename_;
};

#endif // IMAGE_VIEWER_H_
