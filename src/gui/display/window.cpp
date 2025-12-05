#include "gui/display/window.h"

#include <SFML/System/Err.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/Window.hpp>

#ifdef _WIN32
    #include <windows.h> //https://stackoverflow.com/questions/12061401/how-to-make-a-c-program-run-maximized-automatically
#elif defined(__linux__) //https://stackoverflow.com/questions/4530786/xlib-create-window-in-mimized-or-maximized-state
    #include <X11/Xlib.h>
#endif

namespace WindowUtils {
    void maximize(sf::Window& window) {
        auto handle = window.getNativeHandle();

        #ifdef _WIN32
            ShowWindow(handle, SW_MAXIMIZE);
        #elif defined(__linux__)
            Display* display = XOpenDisplay(nullptr);
            if (display) {
                ::Window win = static_cast<::Window>(handle);

                Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
                Atom maxH = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
                Atom maxV = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

                XEvent xev;
                xev.type = ClientMessage;
                xev.xclient.window = win;
                xev.xclient.message_type = wmState;
                xev.xclient.format = 32;
                xev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
                xev.xclient.data.l[1] = maxV;
                xev.xclient.data.l[2] = maxH;

                XSendEvent(display, DefaultRootWindow(display), False,
                           SubstructureRedirectMask | SubstructureNotifyMask, &xev);
                
                XFlush(display);
                XCloseDisplay(display);
            }
        #endif
    }

    sf::RenderWindow createWindow()
    {
        auto previous = sf::err().rdbuf();
        sf::err().rdbuf(nullptr); // Nuke that no sync
        sf::RenderWindow window(
                        sf::VideoMode(sf::Vector2u(800, 600)),
                        "QuestiMakinator",
                        sf::Style::Default,
                        sf::State::Windowed,
                        {}
        );
        sf::err().rdbuf(previous);
        
        window.setFramerateLimit(60);
        window.setTitle("QuestiMakinator");
        WindowUtils::maximize(window);
        return window;
    }
}