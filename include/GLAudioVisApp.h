#pragma once

#include <SDL2/SDL.h>
#include <GL/glew.h> // load glew before SDL_opengl

#include <imgui/imgui.h>

#include "SDLUtils/Window.h"
#include "SDLUtils/GLContext.h"

#include "GLUtils/ShaderProgram.h"
#include "GLUtils/VAO.h"

#include "AudioEngine.h"

namespace gaz
{

class GLAudioVisApp
{
public:
	// This it the only entry point, and takes control of execution
	// returns exit code back to main.cpp
	static int execute(int argc, char* argv[]);

private:
	// Constructors
	GLAudioVisApp() :
		m_mainWindow{nullptr},
		m_glContext{nullptr},
		m_imGuiContext{nullptr},
		m_audioEngine
		({
			2, // numChannels
			48000, // sampleRate
			1024, // numSamples
			PA_SAMPLE_FLOAT32LE, // sample format
		}),
		m_outputShader{nullptr},
		m_emptyVAO{nullptr}
	{
		fmt::print("GLAudioVisApp()\n");
	}

	// Disable copy constructor and assignment operator, since we're managing OpenGL resources, and it's
	// not worth the hassle to share their ownership
	GLAudioVisApp(const GLAudioVisApp&) = delete;
	GLAudioVisApp& operator=(const GLAudioVisApp&) = delete;
	// ...and move constructor, move assignment
	GLAudioVisApp(GLAudioVisApp&&) = delete;
	GLAudioVisApp& operator=(GLAudioVisApp&&) = delete;

	// Destructors
	~GLAudioVisApp();

	// free SDL and OpenGL resources
	void cleanup();

	// Initialisation functions

	// initialise SDL and OpenGL
	bool init();

	bool initSDLWindow();

	bool initGLContext();

	bool initImGuiContext();

	bool initPulseAudioSource();

	bool initDrawingPipeline(); // shaders & gl state

	// Main program loop
	void run();

	// event handling
	void processEvent(const SDL_Event& event);

	// rendering
	void drawFrame();

	void drawGUI();

	// SDL Window object
	std::unique_ptr<SDLUtils::Window> m_mainWindow;

	// OpenGL context returned by SDL window
	std::unique_ptr<SDLUtils::GLContext> m_glContext;

	// ImGui context
	ImGuiContext* m_imGuiContext;

	// Audio Engine which does recording, fft, on a seperate thread
	AudioEngine m_audioEngine;

	// pretty pretty
	std::unique_ptr<const GLUtils::ShaderProgram> m_outputShader;

	// Empty vao since we can't draw without one bound in core
	std::unique_ptr<const GLUtils::VAO> m_emptyVAO;
};

}