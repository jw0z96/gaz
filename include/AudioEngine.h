#pragma once

#include <imgui/imgui.h>

#include <pulse/simple.h>

#include <fmt/core.h>

#include <fftw3.h>

#include <vector>
#include <thread>
#include <optional>
#include <mutex>

namespace gaz
{

class AudioEngine
{
public:

	enum struct Channel
	{
		Left = 0, // default for mono
		Right = 1
	};

	struct SamplingSettings
	{
		const unsigned char numChannels; // 1 mono, 2 stereo
		const unsigned int sampleRate; // samples per second
		const unsigned int numSamples; // number of samples or 'frames'
		const pa_sample_format_t sampleFormat; // the size of a sample
	};

	AudioEngine(const SamplingSettings& settings) :
		m_samplingSettings{settings},
		m_source{nullptr},
		m_sampleBuffer{},
		m_recordingActive{false},
		m_recordingThread{nullptr},
		m_numSpectrumBuckets{20},
		m_fftData{},
		m_dftOutputCombined{},
		m_dftOutputReady{false},
		m_histogramSmoothing{0.0f}
	{
		fmt::print("AudioEngine()\n");
		assert(m_samplingSettings.numChannels <= 2); // to match the Channel enum
	}

	~AudioEngine();

	// Disable copy constructor and assignment operator, since we're managing fftw resources, and it's
	// not worth the hassle to share their ownership
	AudioEngine(const AudioEngine&) = delete;
	AudioEngine& operator=(const AudioEngine&) = delete;
	// ...and move constructor, move assignment
	AudioEngine(AudioEngine&&) = delete;
	AudioEngine& operator=(AudioEngine&&) = delete;

	bool init();

	void toggleRecording();

	bool isRecordingActive() const { return m_recordingActive; }

	const SamplingSettings& getSamplingSettings() const { return m_samplingSettings; }

	// ImGui Helper Functions
	void plotInputPCM(
		const Channel& channel,
		const char* label,
		const char* overlay,
		const ImVec2& size
	);

	void plotDFT(
		const Channel& channel,
		const char* label,
		const char* overlay,
		const ImVec2& size
	);

	void plotSpectrum(
		const Channel& channel,
		const char* label,
		const char* overlay,
		const ImVec2& size
	);

	// Histogram display controls
	void setSpectrumBucketCount(unsigned int bucketCount);

	unsigned int getSpectrumBucketCount() const { return m_numSpectrumBuckets; }

	void setHistogramSmoothing(float smoothing) { m_histogramSmoothing = smoothing; }

	float getHistogramSmoothing() const { return m_histogramSmoothing; }

	// Access for OpenGL buffers
	const std::vector<float>& getDFT(const Channel& channel) const;

	// returns a copy of the combined DFT, if it's ready
	const std::optional<const std::vector<float>> requestDFT(/*const Channel& channel*/);

private:

	void startRecording();

	// static std::vector<float> calculateBuckets(int numBuckets, float powerCurve);

	const SamplingSettings m_samplingSettings;

	// PulseAudio audio source connection
	pa_simple* m_source;

	std::vector<char> m_sampleBuffer; // use char here, as 1 byte

	bool m_recordingActive;
	std::unique_ptr<std::thread> m_recordingThread;

	int m_numSpectrumBuckets;

	struct FFTData
	{
		Channel channelID;
		// internal data for fftW
		std::vector<double> fftwInput;
		fftw_complex* fftwOutput;
		fftw_plan fftwPlan;
		// Processed output data

		// bool dftOutputChanged;
		std::vector<float> dftOutputRaw;
		std::vector<float> spectrumBuckets;
	};

	std::vector<FFTData> m_fftData;

	std::vector<float> m_dftOutputCombined;
	bool m_dftOutputReady;

	float m_histogramSmoothing;
};

}
