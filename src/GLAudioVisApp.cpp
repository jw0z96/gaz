#include "GLAudioVisApp.h"

#include <fmt/core.h>

#include <SDL2/SDL_opengl.h>

#include <imgui/imgui_impl_sdl.h>
#include <imgui/imgui_impl_opengl3.h>

#include <pulse/error.h>

#include <chrono>

#include "GLUtils/Timer.h"
namespace
{
	constexpr unsigned int DEFAULT_SCREEN_WIDTH = 1024;
	constexpr unsigned int DEFAULT_SCREEN_HEIGHT = 768;

	constexpr float freqBuckets[] = { 20.0f, 140.0f, 400.0f, 2600.0f, 5200.0f, std::numeric_limits<float>::max() };

	float runLoopElapsed = 0.0f;

	float audioProcessingTime1 = 0.0f;
	float audioProcessingTime2 = 0.0f;
	float audioProcessingTime3 = 0.0f;
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

	for (const auto &processedAudioData : m_processedAudioData)
	{
		fftw_free(processedAudioData.fftwOutput);
		fftw_destroy_plan(processedAudioData.fftwPlan);
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

	sampleFormat.format = m_audioSamplingSettings.sampleFormat;
	sampleFormat.channels = m_audioSamplingSettings.numChannels;
	sampleFormat.rate = m_audioSamplingSettings.sampleRate;

	// pa_buffer_attr ba;
	// ba.maxlength = 1024; // max length of the buffer
	// ba.tlength = (uint32_t)-1; // target buffer length (bytes) ?  playback only?
	// ba.minreq = 1024; // minimum request ?
	// ba.fragsize = 1024; // fragment size (bytes) recording only?

	// TODO: command line option
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

	// resize the buffer to accomodate for the read size (bytes)
	// m_audioSamplingSettings.numSamples = sampleFormat.rate; // 1 sec of audio samples
	const size_t bufferSize = pa_sample_size_of_format(m_audioSamplingSettings.sampleFormat) * m_audioSamplingSettings.numSamples * m_audioSamplingSettings.numChannels;
	fmt::print("buffer size: {}\n", bufferSize);
	m_audioSampleBuffer.resize(bufferSize);

	// Since we need to keep the references passed to fftwPlan intact, default construct 'numchannels' elements,
	// then fill them
	m_processedAudioData.resize(m_audioSamplingSettings.numChannels);
	for (size_t i = 0; i < m_audioSamplingSettings.numChannels; ++i)
	{
		AudioProcessData& data = m_processedAudioData[i];
		data.channelID = static_cast<AudioChannel>(i);
		// Prepare data for fftw
		data.fftwInput.resize(m_audioSamplingSettings.numSamples);
		data.fftwOutput = fftw_alloc_complex(sizeof(fftw_complex) * m_audioSamplingSettings.numSamples);
		data.fftwPlan = fftw_plan_dft_r2c_1d(
			m_audioSamplingSettings.numSamples, data.fftwInput.data(), data.fftwOutput, FFTW_PATIENT | FFTW_DESTROY_INPUT
		);
		// Allocate vectors that are used by ImGui
		data.dftOutputRaw.resize(m_audioSamplingSettings.numSamples / 2);
		data.spectrumBuckets.fill(0.0f);
	}

	return true;
}

void GLAudioVisApp::run()
{
	// main loop
	SDL_Event event;
	const auto& mainWindowRaw = m_mainWindow->get();
	while (true)
	{
		const auto start = std::chrono::system_clock::now();

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

		// Capture audio and perform FFT
		processAudio();

		// our opengl render
		drawFrame();

		// Start the ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(mainWindowRaw);
		ImGui::NewFrame();

		// Populate the ImGui frame with scene info
		drawGUI();

		// Draw the ImGui frame
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(mainWindowRaw);

		const auto end = std::chrono::system_clock::now();
		runLoopElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	}
}

void GLAudioVisApp::processEvent(const SDL_Event&)
{
}

void GLAudioVisApp::processAudio()
{
	const auto startTime1 = std::chrono::system_clock::now();

	if (m_audioThreadActive)
	{
		const auto startTime2 = std::chrono::system_clock::now();

		// This will block for a fixed amount of time
		int error;
		if (pa_simple_read(m_audioSource, m_audioSampleBuffer.data(), m_audioSampleBuffer.size(), &error) < 0)
		{
			fmt::print("GLAudioVisApp::processAudio: Failed to read: {}\n", pa_strerror(error));
			return;
		}

		const auto endTime2 = std::chrono::system_clock::now();

		// If we have more than 1 channel, unpack into the different data channels
		const size_t numChannels = m_processedAudioData.size();
		if (numChannels > 1)
		{
			// reinterpret as float array as it should match the sample size
			assert(sizeof(float) == pa_sample_size_of_format(m_audioSamplingSettings.sampleFormat));
			const float* buf = reinterpret_cast<float*>(m_audioSampleBuffer.data());
			for (unsigned int i = 0; i < m_audioSamplingSettings.numSamples; ++i)
			{
				for (size_t j = 0; j < numChannels; ++j)
				{
					m_processedAudioData[j].fftwInput[i] = buf[numChannels * i + j];
				}
			}
		}

		// used for determining approx frequencies from the DFT sample index
		static const float reciprocal = static_cast<float>(m_audioSamplingSettings.sampleRate) / static_cast<float>(m_audioSamplingSettings.numSamples);

		// put these on seperate threads?
		for (auto& fftData : m_processedAudioData)
		{
			// run the DFT
			fftw_execute(fftData.fftwPlan);

			// first lower the values in the buckets by the smoothing factor
			for (auto& bucket : fftData.spectrumBuckets)
			{
				bucket *= m_histogramSmoothing;
			}

			// we only care about samples in the DFT that are below the nyquist frequency (midpoint)
			for (unsigned int i = 0; i < (m_audioSamplingSettings.numSamples / 2); ++i)
			{
				const fftw_complex& sample = fftData.fftwOutput[i];
				const float amplitude = sqrt(sample[0] * sample[0] + sample[1] * sample[1]);
				// const float amplitude = sample[0] + sample[1]; // no need to sqrt
				fftData.dftOutputRaw[i] = amplitude;

				// Frequency is approximate, based on the sample size
				const float freq = static_cast<float>(i) * reciprocal;
				for (size_t j = 0; j < s_numBuckets; ++j)
				{
					// freqBuckets is sized s_numBuckets + 1, so this is safe
					if (freq > freqBuckets[j] && freq < freqBuckets[j + 1])
					{
						fftData.spectrumBuckets[j] = amplitude > fftData.spectrumBuckets[j] ?
							amplitude :
							fftData.spectrumBuckets[j];
						break;
					}
				}
			}

		}

		const auto endTime3 = std::chrono::system_clock::now();

		audioProcessingTime2 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime2 - startTime2).count();
		audioProcessingTime3 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime3 - endTime2).count();
	}

	const auto endTime1 = std::chrono::system_clock::now();
	audioProcessingTime1 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime1 - startTime1).count();

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

	const auto& currentWidth = ImGui::GetWindowWidth();

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

		ImGui::SetNextItemWidth(currentWidth);
		ImGui::PlotLines("##FrameTimes", frameTimes, numFrameSamples, frameOffset, overlay.c_str(), 0.0f, 100.0f, ImVec2(0,80));

		frameOffset = (frameOffset + 1) % numFrameSamples;
	}

	ImGui::Separator();

	ImGui::Text("Audio Sample Size: %lu", pa_sample_size_of_format(m_audioSamplingSettings.sampleFormat));
	ImGui::Text("Audio Samples: %u", m_audioSamplingSettings.numSamples);

	ImGui::Separator();

	ImGui::Text("Run Loop Time: %.1fms", runLoopElapsed);
	ImGui::Text("\tAudio Processing Time Total: %.1fms", audioProcessingTime1);
	ImGui::Text("\t\tTime 1: %fms", audioProcessingTime2);
	ImGui::Text("\t\tTime 2: %fms", audioProcessingTime3);

	{
		if (ImGui::Button(!m_audioThreadActive ? "Start Recording" : "Stop Recording"))
		{
			toggleRecording();
		}

		// Cast the start of the buffer to a float array, since that's what ImGui needs to plot lines, and sizeof(float)
		// matches m_bytesPerSample
		assert(sizeof(float) == pa_sample_size_of_format(m_audioSamplingSettings.sampleFormat));
		const float* buf = reinterpret_cast<float*>(m_audioSampleBuffer.data());

		// Left and Right sample data is interleaved - Left starts at [0], Right starts at [1], 2 element stride
		ImGui::SetNextItemWidth(currentWidth * 0.5f);
		ImGui::PlotLines(
			"##AudioSamplesL",
			&buf[0],
			m_audioSamplingSettings.numSamples / 2,
			0,
			"Raw PCM (L)",
			-1.0f,
			1.0f,
			ImVec2(0,80),
			sizeof(float) * 2 // stride
		);
		ImGui::SameLine();

		ImGui::SetNextItemWidth(currentWidth * 0.5f);
		ImGui::PlotLines(
			"##AudioSamplesR",
			&buf[1],
			m_audioSamplingSettings.numSamples / 2,
			0,
			"Raw PCM (R)",
			-1.0f,
			1.0f,
			ImVec2(0,80),
			sizeof(float) * 2 // stride
		);

		for (const auto& fftData : m_processedAudioData)
		{
			ImGui::SetNextItemWidth(currentWidth * 0.5f);
			ImGui::PlotLines(
				fftData.channelID == AudioChannel::Left ?
					"##fftOutputRawL" :
					"##fftOutputRawR",
				fftData.dftOutputRaw.data(),
				fftData.dftOutputRaw.size(),
				0,
				fftData.channelID == AudioChannel::Left ?
					"Raw DFT (L)" :
					"Raw DFT (R)",
				0.0f,
				100.0f,
				ImVec2(0, 80)
			);
			ImGui::SameLine();
		}

		ImGui::NewLine(); ImGui::Separator();

		ImGui::SetNextItemWidth(currentWidth);
		ImGui::SliderFloat("##Smoothing", &m_histogramSmoothing, 0.0f, 1.0f, "Histogram Smoothing: %.1f");

		for (const auto& fftData : m_processedAudioData)
		{
			ImGui::SetNextItemWidth(currentWidth * 0.5f);
			ImGui::PlotHistogram(
				fftData.channelID == AudioChannel::Left ?
					"##AudioHistogramL" :
					"##AudioHistogramR",
				fftData.spectrumBuckets.data(),
				s_numBuckets,
				0,
				fftData.channelID == AudioChannel::Left ? "L" : "R",
				0.0f,
				100.0f,
				ImVec2(0, 80.0f)
			);
			ImGui::SameLine();
		}

		ImGui::NewLine();
	}

	ImGui::End();
}