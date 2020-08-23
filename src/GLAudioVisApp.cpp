#include "GLAudioVisApp.h"

#include <fmt/core.h>

#include <SDL2/SDL_opengl.h>

#include <imgui/imgui_impl_sdl.h>
#include <imgui/imgui_impl_opengl3.h>

#include <pulse/error.h>

#include "GLUtils/Timer.h"

namespace
{
	constexpr unsigned int DEFAULT_SCREEN_WIDTH = 1024;
	constexpr unsigned int DEFAULT_SCREEN_HEIGHT = 768;
};

int GLAudioVisApp::execute(int argc, char* argv[])
{
	if (argc > 1)
	{
		fmt::print("Command line args:\n");
		for (int i = 0; i < argc; ++i)
		{
			fmt::print("\t{} : {}\n", i, argv[i]);
		}
	}

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		fmt::print("SDL System cannot init with error: {}\n", SDL_GetError());
		// Close SDL subsystems
		SDL_Quit();
		return EXIT_FAILURE;
	}
	else // Scoped to ensure GLAudioVisApp dtor is called before SDL_Quit
	{
		GLAudioVisApp app;
		// handle init failure
		if (!app.init())
		{
			fmt::print("GLAudioVisApp Failed to init - exiting\n");
			// Close SDL subsystems
			SDL_Quit();
			return EXIT_FAILURE;
		}
		else
		{
			app.run();
		}
	}

	// Close SDL subsystems
	SDL_Quit();
	return EXIT_SUCCESS;
}

GLAudioVisApp::~GLAudioVisApp()
{
	fmt::print("GLAudioVisApp::~GLAudioVisApp\n");

	GLUtils::clearTimers();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext(m_imGuiContext);

	if (m_audioSource != nullptr)
	{
		// make sure the recording thread is closed
		// TODO: this is messy, maybe use async & future?
		if (m_audioThreadActive)
		{
			m_audioThreadActive = false;
			// m_audioThread.join();
		}

		pa_simple_free(m_audioSource);
	}
}

bool GLAudioVisApp::init()
{
	if (!initSDLWindow())
	{
		fmt::print(
			"GLAudioVisApp::init: Failed to create SDL window, error: {}\n",
			SDL_GetError()
		);
		return false;
	}

	if (!initGLContext())
	{
		fmt::print(
			"GLAudioVisApp::init: Failed to create OpenGL context\n"
		);
		return false;
	}

	if (!initImGuiContext())
	{
		fmt::print(
			"GLAudioVisApp::init: Failed to create ImGui context\n"
		);
		return false;
	}

	if (!initPulseAudioSource())
	{
		fmt::print(
			"GLAudioVisApp::init: Failed to create PulseAudio source\n"
		);
		return false;
	}


	return true;
}

bool GLAudioVisApp::initSDLWindow()
{
	m_mainWindow = std::make_unique<SDLUtils::Window>("GLAudioVisApp",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		DEFAULT_SCREEN_WIDTH,
		DEFAULT_SCREEN_HEIGHT,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN // | SDL_WINDOW_RESIZABLE
	);

	return m_mainWindow != nullptr ? m_mainWindow->valid() : false;
}

bool GLAudioVisApp::initGLContext()
{
	// set opengl version to use in this program, 4.2 core profile (until I find a deprecated function to use)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3); // :)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// use multisampling?
	// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	m_glContext = std::make_unique<SDLUtils::GLContext>(m_mainWindow->get());
	if (m_glContext == nullptr || !m_glContext->valid())
	{
		fmt::print(
			"GLAudioVisApp::initGLContext: Failed to create GL context with SDL window, error: {}\n",
			SDL_GetError()
		);
		return false;
	}

	// disable vsync in the OpenGL context, TODO: set this in some kind of config?
	SDL_GL_SetSwapInterval(0);

	// initialize GLEW once we have a valid GL context - TODO: GLAD?
	glewExperimental = GL_TRUE;
	GLenum glewError = glewInit();
	if(glewError != GLEW_OK)
	{
		fmt::print(
			"GLAudioVisApp::initGLContext: Failed to init GLEW, error: {}\n",
			glewGetErrorString(glewError)
		);
		return false;
	}

	return true;
}

bool GLAudioVisApp::initImGuiContext()
{
	// Init imgui
	IMGUI_CHECKVERSION();
	ImGuiContext* m_imGuiContext = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(m_mainWindow->get(), m_glContext->get());
	ImGui_ImplOpenGL3_Init("#version 420"); // glsl version

	return m_imGuiContext != nullptr;
}

bool GLAudioVisApp::initPulseAudioSource()
{
	// Specify the sample format, should be possible to determine this from `pacmd list-sources`?
	pa_sample_spec sampleFormat;
	sampleFormat.format = PA_SAMPLE_FLOAT32LE; // PA_SAMPLE_S16LE; // 2 byte sample
	sampleFormat.channels = 2;
	// sampleFormat.rate = 44100;
	sampleFormat.rate = 48000;

	// pa_buffer_attr ba;
	// ba.maxlength = 1024; // max length of the buffer
	// ba.tlength = (uint32_t)-1; // target buffer length (bytes) ?  playback only?
	// ba.minreq = 1024; // minimum request ?
	// ba.fragsize = 1024; // fragment size (bytes) recording only?

	// const std::string source = "alsa_input.pci-0000_00_1b.0.analog-stereo";
	const std::string source = "alsa_output.pci-0000_00_1b.0.analog-stereo.monitor";

	// connect to the PulseAudio server
	int error;
	m_audioSource = pa_simple_new(
		nullptr,			// Use the default server.
		"GLAudioVis",		// Our application's name.
		PA_STREAM_RECORD,	// Connection Mode
		source.c_str(),		// Use the default device.
		"Record",			// Description of our stream.
		&sampleFormat,		// Our sample format.
		nullptr,			// Use default channel map
		nullptr,			// Use default buffering attributes.
		&error				// Ignore error code.
	);

	if (m_audioSource == nullptr || error < 0)
	{
		fmt::print(
			"GLAudioVisApp::initPulseAudioSource: Failed to connect to audio source '{}', error: {}\n",
			source.c_str(),
			pa_strerror(error)
		);
		return false;
	}

	m_bytesPerSample = pa_sample_size(&sampleFormat); // should be 4 bytes as we request 32 bit float

	// resize the buffer to accomodate for the read size (bytes)
	m_audioSampleBuffer.clear();
	const size_t bufferSize = m_bytesPerSample * m_numAudioSamples;
	fmt::print("buffer size: {}\n", bufferSize);
	m_audioSampleBuffer.resize(bufferSize);


	return true;
}

void GLAudioVisApp::run()
{
	// main loop
	SDL_Event event;
	while (true)
	{
		// Event handling
		while (SDL_PollEvent(&event) != 0)
		{
			if (event.type == SDL_QUIT ||
				(event.type == SDL_KEYDOWN &&
				event.key.keysym.sym == SDLK_ESCAPE))
			{
				fmt::print("GLAudioVisApp::run: exit signal received\n");
				return;
			}

			processEvent(event);
		}

		processAudio();

		// our opengl render
		drawFrame();

		// Start the ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_mainWindow->get());
		ImGui::NewFrame();
		// Populate the ImGui frame with scene info
		drawGUI();
		// Draw the ImGui frame
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(m_mainWindow->get());
	}
}

void GLAudioVisApp::processEvent(const SDL_Event&)
{
}

void GLAudioVisApp::processAudio()
{
	int error;
	/*
	fmt::print("GLAudioVisApp::processAudio: Started...\n");

	while (m_audioThreadActive)
	{
		*/
		if (m_audioThreadActive)
		{
			if (pa_simple_read(m_audioSource, m_audioSampleBuffer.data(), m_audioSampleBuffer.size(), &error) < 0)
			{
				fmt::print("GLAudioVisApp::processAudio: Failed to read: {}\n", pa_strerror(error));
				// return;
			}
		}
		/*
	}

	fmt::print("GLAudioVisApp::processAudio: Ended...\n");
	*/
}

void GLAudioVisApp::toggleRecording()
{
	m_audioThreadActive = !m_audioThreadActive;

	// if (m_audioThreadActive)
	// {
	// 	m_audioThread = std::thread(&GLAudioVisApp::processAudio, this);
	// }
	// else
	// {
	// 	m_audioThread.join();
	// }
}

void GLAudioVisApp::drawFrame()
{
	GLUtils::scopedTimer(frameTimer);

	glClear(GL_COLOR_BUFFER_BIT);
}

void GLAudioVisApp::drawGUI()
{
	if (!ImGui::Begin("Stats"))
	{
		// Early out if the window is collapsed
		ImGui::End();
		return;
	}

	const float frameTime = GLUtils::getElapsed(frameTimer);
	ImGui::Text("Frame time: %.1f ms (%.1f fps)", frameTime, 1000.0f / frameTime);

	// create a plot of the frame times
	{
		// 100 samples will be plotted in a line
		constexpr unsigned int numFrameSamples = 50;
		constexpr float numFrameSamplesReciprocal = 1.0f / (float)numFrameSamples;
		static float frameTimes[numFrameSamples] = { 0.0f };
		// this will be used as an index to update with the latest data, and to rotate the array in the display
		static unsigned int frameOffset = 0;

		frameTimes[frameOffset] = frameTime;

		// average the frame samples
		float average = 0.0f;
		for (unsigned int n = 0; n < numFrameSamples; n++)
		{
			average += frameTimes[n];
		}
		average *= numFrameSamplesReciprocal;

		const auto overlay = fmt::format("Average {0:.1f} ms ({0:.1f} fps)", average, 1000.0f / average);
		ImGui::PlotLines("##FrameTimes", frameTimes, numFrameSamples, frameOffset, overlay.c_str(), 0.0f, 100.0f, ImVec2(0,80));

		frameOffset = (frameOffset + 1) % numFrameSamples;
	}

	ImGui::Separator();

	ImGui::Text("Audio Sample Size: %u", m_bytesPerSample);
	ImGui::Text("Audio Samples: %u", m_numAudioSamples);

	{
		if (ImGui::Button(!m_audioThreadActive ? "Start Recording" : "Stop Recording"))
		{
			toggleRecording();
		}

		// Cast the start of the buffer to a float array, since that's what ImGui needs to plot lines, and sizeof(float)
		// matches m_bytesPerSample
		const float* buf = reinterpret_cast<float*>(m_audioSampleBuffer.data());

		// Left and Right sample data is interleaved - Left starts at [0], Right starts at [1], 2 element stride
		ImGui::PlotLines(
			"##AudioSamples",
			&buf[0],
			m_numAudioSamples / 2,
			0,
			"Raw PCM (L)",
			-1.0f,
			1.0f,
			ImVec2(0,80),
			sizeof(float) * 2 // stride
		);

		ImGui::PlotLines(
			"##AudioSamples",
			&buf[1],
			m_numAudioSamples / 2,
			0,
			"Raw PCM (R)",
			-1.0f,
			1.0f,
			ImVec2(0,80),
			sizeof(float) * 2 // stride
		);
	}

	ImGui::End();
}