#pragma once

#include <thread>

#include <SDL2/SDL.h>
#include <GL/glew.h> // load glew before SDL_opengl
#include <pulse/simple.h>
#include <fftw3.h>

#include <fmt/core.h>
#include <imgui/imgui.h>

#include "SDLUtils/Window.h"
#include "SDLUtils/GLContext.h"

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
		m_audioSource{nullptr},
		m_bytesPerSample{1}, // char
		m_numAudioSamples{1024},
		m_audioSampleBuffer{},
		m_audioThreadActive{false},
		m_audioThread{},
		m_dftPlanLeft{nullptr},
		m_dftPlanRight{nullptr},
		m_fftInputLeft{},
		m_fftInputRight{},
		m_fftOutputLeft{nullptr},
		m_fftOutputRight{nullptr},
		m_leftSampleBuckets{},
		m_rightSampleBuckets{},
		m_histogramSmoothing{0.0f}
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

	// Main program loop
	void run();

	// event handling
	void processEvent(const SDL_Event& event);

	void processAudio();

	void toggleRecording();

	// rendering
	void drawFrame();

	void drawGUI();

	// SDL Window object
	std::unique_ptr<SDLUtils::Window> m_mainWindow;

	// OpenGL context returned by SDL window
	std::unique_ptr<SDLUtils::GLContext> m_glContext;

	// ImGui context
	ImGuiContext* m_imGuiContext;

	// PulseAudio audio source connection
	pa_simple* m_audioSource;

	unsigned int m_bytesPerSample;
	unsigned int m_numAudioSamples;
	std::vector<char> m_audioSampleBuffer; // use char here, as 1 byte
	bool m_audioThreadActive;
	std::thread m_audioThread;

	fftw_plan m_dftPlanLeft, m_dftPlanRight;
	std::vector<double> m_fftInputLeft, m_fftInputRight;
	fftw_complex* m_fftOutputLeft;
	fftw_complex* m_fftOutputRight;

	static constexpr size_t s_numBuckets = 5;
	std::array<float, s_numBuckets> m_leftSampleBuckets;
	std::array<float, s_numBuckets> m_rightSampleBuckets;

	float m_histogramSmoothing;
};