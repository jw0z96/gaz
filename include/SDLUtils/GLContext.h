#pragma once

#include <SDL2/SDL_opengl.h>

#include <fmt/core.h>

namespace SDLUtils
{
	class GLContext
	{
	public:
		explicit GLContext(SDL_Window* window)
			: m_GLContext(SDL_GL_CreateContext(window))
		{
			// TODO: exceptions on creation failure?
			fmt::print("SDLUtils::GLContext()\n");
		}

		~GLContext()
		{
			fmt::print("SDLUtils::~GLContext\n");
			if (m_GLContext != nullptr)
			{
				SDL_GL_DeleteContext(m_GLContext);
			}
		}

		// Disable copy constructor and assignment operator, since we're managing SDL resources, and it's
		// not worth the hassle to share their ownership
		GLContext(const GLContext&) = delete;
		GLContext& operator=(const GLContext&) = delete;
		// ...and move constructor, move assignment
		GLContext(GLContext&&) = delete;
		GLContext& operator=(GLContext&&) = delete;

		// Returns whether the GLContext is valid
		bool valid() const { return m_GLContext != nullptr; }

		// Get the GLContext object, for passing to other SDL functions
		SDL_GLContext get() const { return m_GLContext; };

	private:
		// The SDL GLContext resource (SDL_GLContext is alias for void*)
		SDL_GLContext m_GLContext;
	};
}