#include "GLAudioVisApp.h"

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <SDL2/SDL_opengl.h>

#include <imgui/imgui_impl_sdl.h>
#include <imgui/imgui_impl_opengl3.h>

#include <chrono>

#include "GLUtils/Timer.h"

namespace
{
	constexpr unsigned int DEFAULT_SCREEN_WIDTH = 800; // 1024;
	constexpr unsigned int DEFAULT_SCREEN_HEIGHT = 600; // 768;

	float runLoopElapsed = 0.0f;
};

using namespace gaz;

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

	if (m_imGuiContext != nullptr)
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext(m_imGuiContext);
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

	if (!initDrawingPipeline())
	{
		fmt::print(
			"GLAudioVisApp::init: Failed to configure drawing pipeline\n"
		);
		return false;
	}

	if (!m_audioEngine.init())
	{
		fmt::print(
			"GLAudioVisApp::init: Failed to init Audio Engine\n"
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
	SDL_GL_SetSwapInterval(1);

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

bool GLAudioVisApp::initDrawingPipeline()
{
	m_outputShader = std::make_unique<const GLUtils::ShaderProgram>(
		std::list<GLUtils::ShaderProgram::ShaderComponent>
		{
			{ GL_VERTEX_SHADER, "shaders/screenspace.vert" },
			{ GL_FRAGMENT_SHADER, "shaders/output.frag" }
		}
	);

	if (!m_outputShader->isValid())
	{
		fmt::print("GLAudioVisApp::initDrawingPipeline: output shader invalid!\n");
		return false;
	}

	m_outputShader->use();

	m_emptyVAO = std::make_unique<const GLUtils::VAO>();
	m_emptyVAO->bind();

	glDisable(GL_DEPTH_TEST);

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
	// pass through to m_audioEngine?
}

void GLAudioVisApp::drawFrame()
{
	GLUtils::scopedTimer(frameTimer);

	glClear(GL_COLOR_BUFFER_BIT);

	m_outputShader->use();

	// TODO: SoA rather than AoS?
/*
	{
		GLUtils::scopedTimer(uniformTimer);

		constexpr size_t dftSize = 512;

		static const auto dftLeftLoc = m_outputShader->getUniformLocation("u_dftLeft[0]");
		glUniform1fv(dftLeftLoc, dftSize, m_audioFFTData[0].dftOutputRaw.data());
		// glUniform1fv(dftLeftLoc, dftSize, m_audioFFTData[0].spectrumBuckets.data());

		static const auto dftRightLoc = m_outputShader->getUniformLocation("u_dftRight[0]");
		glUniform1fv(dftRightLoc, dftSize, m_audioFFTData[1].dftOutputRaw.data());
		// glUniform1fv(dftRightLoc, dftSize, m_audioFFTData[1].spectrumBuckets.data());

	}
*/
	// The vertex shader will create a screen space quad, so no need to bind a different VAO & VBO
	glDrawArrays(GL_TRIANGLES, 0, 6);
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

	ImGui::Text("Run Loop Time: %.1fms", runLoopElapsed);

	ImGui::Separator();

	const float frameTime = GLUtils::getElapsed(frameTimer);
	ImGui::Text("Frame time: %.1f ms (%.1f fps)", frameTime, 1000.0f / frameTime);
	ImGui::Text("\tUniform update time: %.1fms", GLUtils::getElapsed(uniformTimer));

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
/*
	ImGui::Text("Audio Sample Size: %lu", pa_sample_size_of_format(m_audioSamplingSettings.sampleFormat));
	ImGui::Text("Audio Samples: %u", m_audioSamplingSettings.numSamples);

	ImGui::NewLine();

	ImGui::Text("Audio Processing Time Total: %.1fms", audioProcessingTime1);
	ImGui::Text("\tTime 1: %fms", audioProcessingTime2);
	ImGui::Text("\tTime 2: %fms", audioProcessingTime3);
*/
	{
		if (ImGui::Button(!m_audioEngine.isRecordingActive() ? "Start Recording" : "Stop Recording"))
		{
			m_audioEngine.toggleRecording();
		}

		/*
		bool spectrumChanged = false;
		spectrumChanged = ImGui::SliderInt("##NumBuckets", &m_numSpectrumBuckets, 1, 100, "Num Spectrum Buckets: %i");
		if (spectrumChanged)
		{
			for (auto& fftData : m_audioFFTData)
			{
				fftData.spectrumBuckets.clear();
				fftData.spectrumBuckets.resize(m_numSpectrumBuckets);
			}
		}
		// spectrumChanged = spectrumChanged || ImGui::SliderFloat("##PowerCurve", &m_spectrumPowerCurve, 0.0f, 10.0f, "Spectrum Power Curve: %.2f");
		// if (spectrumChanged)
		// {
		// 	m_spectrumBuckets = calculateBuckets(m_numSpectrumBuckets, m_spectrumPowerCurve);
		// 	// fmt::print("buckets: {}\n", m_spectrumBuckets);
		// }

		ImGui::SetNextItemWidth(currentWidth);
		ImGui::SliderFloat("##Smoothing", &m_histogramSmoothing, 0.0f, 1.0f, "Histogram Smoothing: %.1f");

		// Cast the start of the buffer to a float array, since that's what ImGui needs to plot lines, and sizeof(float)
		// matches m_bytesPerSample
		assert(sizeof(float) == pa_sample_size_of_format(m_audioSamplingSettings.sampleFormat));
		const float* buf = reinterpret_cast<float*>(m_audioSampleBuffer.data());

		ImGui::Columns(m_audioSamplingSettings.numChannels);
		for (unsigned int i = 0; i < m_audioSamplingSettings.numChannels; ++i)
		{
			const auto& columnWidth = ImGui::GetColumnWidth();
			const auto& fftData = m_audioFFTData[i];
			const char* channelNameShort = fftData.channelID == AudioChannel::Left ? "L" : "R";
			const char* channelNameLong = fftData.channelID == AudioChannel::Left ? "Left" : "Right";

			ImGui::Text("%s (%s)", channelNameShort, channelNameLong);

			// Raw PCM
			ImGui::PlotLines(
				fmt::format("##AudioSamples{}", channelNameShort).c_str(),
				&buf[i],
				m_audioSamplingSettings.numSamples / 2,
				0,
				fmt::format("Raw PCM ({})", channelNameShort).c_str(),
				-1.0f,
				1.0f,
				ImVec2(columnWidth, 80),
				sizeof(float) * 2 // stride
			);

			// Raw DFT
			ImGui::PlotLines(
				fmt::format("##fftOutputRaw{}", channelNameShort).c_str(),
				fftData.dftOutputRaw.data(),
				fftData.dftOutputRaw.size(),
				0,
				fmt::format("Raw DFT ({})", channelNameShort).c_str(),
				0.0f,
				48.0f,
				ImVec2(columnWidth, 80)
			);

			// Histogram
			ImGui::PlotHistogram(
				fmt::format("##AudioHistogram{}", channelNameShort).c_str(),
				fftData.spectrumBuckets.data(),
				m_numSpectrumBuckets,
				0,
				fmt::format("Histogram ({})", channelNameShort).c_str(),
				0.0f,
				48.0f,
				ImVec2(columnWidth, 80.0f)
			);

			ImGui::NextColumn();
		}

		ImGui::Columns(1); // reset the column count
		*/
	}

	ImGui::End();
}