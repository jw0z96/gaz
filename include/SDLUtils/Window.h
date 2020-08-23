#pragma once

#include <SDL2/SDL.h>

#include <fmt/core.h>

namespace SDLUtils
{
	class Window
	{
	public:
		explicit Window(
			const char* title = "SDL Window",
			const int& pos_x = SDL_WINDOWPOS_CENTERED,
			const int& pos_y = SDL_WINDOWPOS_CENTERED,
			const int& width = 800,
			const int& height = 600,
			const uint32_t& flags = 0) :
			m_window(SDL_CreateWindow(
				title,
				pos_x,
				pos_y,
				width,
				height,
				flags
			))
		{
			// TODO: exceptions on creation failure?
			fmt::print("SDLUtils::Window()\n");
		}

		~Window()
		{
			fmt::print("SDLUtils::~Window\n");

			if (m_window != nullptr)
			{
				SDL_DestroyWindow(m_window);
			}
		}

		// Disable copy constructor and assignment operator, since we're managing SDL resources, and it's
		// not worth the hassle to share their ownership
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;
		// ...and move constructor, move assignment
		Window(Window&&) = delete;
		Window& operator=(Window&&) = delete;

		// Returns whether the window is valid
		bool valid() const { return m_window != nullptr; }

		// Get the window pointer, for passing to other SDL functions
		SDL_Window* get() const { return m_window; };

	private:
		// The SDL Window resource
		SDL_Window* m_window;
	};
}