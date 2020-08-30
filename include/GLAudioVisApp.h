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

#include "GLUtils/ShaderProgram.h"
#include "GLUtils/VAO.h"

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
		m_audioSampleBuffer{},
		m_audioThreadActive{false},
		// m_audioThread{},
		m_audioSamplingSettings
		{
			2, // numChannels
			48000, // sampleRate
			1024, // numSamples
			PA_SAMPLE_FLOAT32LE, // sample format
		},
		m_numSpectrumBuckets{20},
		// m_spectrumPowerCurve{2.0f},
		// m_spectrumBuckets
		// {
		// 	calculateBuckets(m_numSpectrumBuckets, m_spectrumPowerCurve)
		// },
		m_audioFFTData{},
		m_histogramSmoothing{0.0f},
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

	// static std::vector<float> calculateBuckets(int numBuckets, float powerCurve);

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

	std::vector<char> m_audioSampleBuffer; // use char here, as 1 byte
	bool m_audioThreadActive;
	std::thread m_audioThread;

	struct AudioSamplingSettings
	{
		const unsigned int numChannels; // 1 mono, 2 stereo
		const unsigned int sampleRate; // samples per second
		const unsigned int numSamples; // number of samples or 'frames'
		const pa_sample_format_t sampleFormat; // the size of a sample
	} m_audioSamplingSettings;

	enum struct AudioChannel
	{
		Left = 0,
		Right = 1
	};

	int m_numSpectrumBuckets;
	// float m_spectrumPowerCurve;
	// std::vector<float> m_spectrumBuckets;

	struct AudioFFTData
	{
		AudioChannel channelID;
		// internal data for fftW
		std::vector<double> fftwInput;
		fftw_complex* fftwOutput;
		fftw_plan fftwPlan;
		// Processed output data
		std::vector<float> dftOutputRaw;
		std::vector<float> spectrumBuckets;
	};

	std::vector<AudioFFTData> m_audioFFTData;

	float m_histogramSmoothing;

	std::unique_ptr<const GLUtils::ShaderProgram> m_outputShader;

	std::unique_ptr<const GLUtils::VAO> m_emptyVAO;
};