#include "AudioEngine.h"

#include <imgui/imgui.h>

#include <pulse/error.h>

#include <cassert>
#include <cmath> // log10

namespace
{
	constexpr float minBucketFreqLog = log10(20.0f);
	constexpr float maxBucketFreqLog = log10(20000.0f);

	// float audioProcessingTime1 = 0.0f;
	// float audioProcessingTime2 = 0.0f;
	// float audioProcessingTime3 = 0.0f;

	// constexpr float minBucketFreqLog = log10(20.0f);
	// constexpr float maxBucketFreqLog = log10(20000.0f);

	// constexpr float freqBuckets[] = { 20.0f, 140.0f, 400.0f, 2600.0f, 5200.0f, std::numeric_limits<float>::max() };
};

using namespace gaz;

AudioEngine::~AudioEngine()
{
	fmt::print("~AudioEngine()\n");

	if (m_source != nullptr)
	{
		// make sure the recording thread is closed
		// TODO: this is messy, maybe use async & future?
		if (m_recordingActive)
		{
			m_recordingActive = false;
			m_recordingThread->join();
		}

		pa_simple_free(m_source);
	}

	for (const auto &processedAudioData : m_fftData)
	{
		fftw_free(processedAudioData.fftwOutput);
		fftw_destroy_plan(processedAudioData.fftwPlan);
	}
}

bool AudioEngine::init()
{
	// Specify the sample format, should be possible to determine this from `pacmd list-sources`?
	pa_sample_spec sampleFormat
	{
		.format = m_samplingSettings.sampleFormat,
		.rate = m_samplingSettings.sampleRate,
		.channels = m_samplingSettings.numChannels
	};

	const unsigned int bufferSize = pa_frame_size(&sampleFormat) * m_samplingSettings.numSamples;

	pa_buffer_attr bufferAttributes
	{
		.maxlength = bufferSize, // max length of the buffer in bytes
		.tlength = (uint32_t)-1, // target buffer length (bytes) ?  playback only?
		.prebuf = (uint32_t)-1, // prebuffering (playback only)
		.minreq = (uint32_t)-1, // minimum request (playback only
		// fragment size (bytes?) (recording only)
		// .fragsize = bufferSize // works, varying bocking times
		.fragsize = 0 // much more consistent
	};

	// TODO: command line option
	// const std::string source = "alsa_input.pci-0000_00_1b.0.analog-stereo";
	const std::string source = "alsa_output.pci-0000_00_1b.0.analog-stereo.monitor";

	// connect to the PulseAudio server
	int error;
	m_source = pa_simple_new(
		nullptr,			// Use the default server
		"GLAudioVisApp",	// Our application's name
		PA_STREAM_RECORD,	// Connection Mode
		source.c_str(),		// Use the specified device
		"Record",			// Description of our stream
		&sampleFormat,		// Our sample format
		nullptr,			// Use default channel map
		&bufferAttributes,	// Use buffering attributes
		&error				// Error code
	);

	if (m_source == nullptr)
	{
		// fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
		fmt::print(
			"AudioEngine::initPulseAudioSource: Failed to connect to audio source '{}', error: {}\n",
			source.c_str(),
			pa_strerror(error)
		);
		return false;
	}

	// resize the buffer to accomodate for the read size (bytes)
	m_sampleBuffer.resize(bufferSize);
	fmt::print("buffer size: {}\n", bufferSize);

	// Since we need to keep the references passed to fftwPlan intact, default construct 'numchannels' elements,
	// then fill them
	m_fftData.resize(m_samplingSettings.numChannels);
	for (size_t i = 0; i < m_samplingSettings.numChannels; ++i)
	{
		FFTData& data = m_fftData[i];
		data.channelID = Channel(i);
		// Prepare data for fftw
		data.fftwInput.resize(m_samplingSettings.numSamples);
		data.fftwOutput = fftw_alloc_complex(sizeof(fftw_complex) * m_samplingSettings.numSamples);
		data.fftwPlan = fftw_plan_dft_r2c_1d(
			m_samplingSettings.numSamples, data.fftwInput.data(), data.fftwOutput, FFTW_PATIENT | FFTW_DESTROY_INPUT
		);

		// Allocate vectors that are used by ImGui / OpenGL
		data.dftOutputRaw.resize(m_samplingSettings.numSamples / 2);
		data.spectrumBuckets.resize(m_numSpectrumBuckets);
	}

	return true;
}

void AudioEngine::toggleRecording()
{
	m_recordingActive = !m_recordingActive;

	if (m_recordingActive)
	{
		m_recordingThread = std::make_unique<std::thread>(&AudioEngine::startRecording, this);
	}
	else
	{
		m_recordingThread->join();
	}
}

void AudioEngine::startRecording()
{
	fmt::print("AudioEngine::startRecording::start\n");

	while (m_recordingActive)
	{
		const auto startTime = std::chrono::system_clock::now();

		// This will block for a fixed amount of time
		int error;
		if (pa_simple_read(m_source, m_sampleBuffer.data(), m_sampleBuffer.size(), &error) < 0)
		{
			fmt::print("AudioEngine::startRecording: Failed to read: {}\n", pa_strerror(error));
			m_recordingActive = false;
			return;
		}

		// If we have more than 1 channel, unpack into the different data channels
		const unsigned int& numChannels = m_samplingSettings.numChannels;
		if (numChannels > 1)
		{
			// reinterpret as float array as it should match the sample size
			assert(sizeof(float) == pa_sample_size_of_format(m_samplingSettings.sampleFormat));
			const float* buf = reinterpret_cast<float*>(m_sampleBuffer.data());
			for (unsigned int i = 0; i < m_samplingSettings.numSamples; ++i)
			{
				for (size_t j = 0; j < numChannels; ++j)
				{
					m_fftData[j].fftwInput[i] = buf[numChannels * i + j];
				}
			}
		}

		// used for determining approx frequencies from the DFT sample index
		static const float reciprocal = static_cast<float>(m_samplingSettings.sampleRate) / static_cast<float>(m_samplingSettings.numSamples);

		// put these on seperate threads?
		for (auto& fftData : m_fftData)
		{
			// run the DFT
			fftw_execute(fftData.fftwPlan);

			// first lower the values in the buckets by the smoothing factor
			for (auto& bucket : fftData.spectrumBuckets)
			{
				bucket *= m_histogramSmoothing;
			}

			// we only care about samples in the DFT that are below the nyquist frequency (midpoint)
			for (unsigned int i = 0; i < (m_samplingSettings.numSamples / 2); ++i)
			{
				const fftw_complex& sample = fftData.fftwOutput[i];
				const float amplitude = 10.0f * log10(sample[0] * sample[0] + sample[1] * sample[1]);
				// const float amplitude = sqrt(sample[0] * sample[0] + sample[1] * sample[1]);
				// const float amplitude = sample[0] + sample[1]; // no need to sqrt
				fftData.dftOutputRaw[i] = amplitude;

				// Frequency is approximate, based on the sample size, so it never fills the buckets properly :/
				const float freq = log10(static_cast<float>(i) * reciprocal);
				float t = (freq - minBucketFreqLog) / (maxBucketFreqLog - minBucketFreqLog);
				int idx = std::max(std::min(int(t * m_numSpectrumBuckets), m_numSpectrumBuckets), 0);
				fftData.spectrumBuckets[idx] = amplitude > fftData.spectrumBuckets[idx] ?
					amplitude :
					fftData.spectrumBuckets[idx];

				// for (int j = 0; j < m_numSpectrumBuckets; ++j)
				// {
				// 	// freqBuckets is sized m_numSpectrumBuckets + 1, so this is safe
				// 	if (freq > m_spectrumBuckets[j] && freq < m_spectrumBuckets[j + 1])
				// 	{
				// 		fftData.spectrumBuckets[j] = amplitude > fftData.spectrumBuckets[j] ?
				// 			amplitude :
				// 			fftData.spectrumBuckets[j];
				// 		break;
				// 	}
				// }
			}
		}

		// std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		const auto endTime = std::chrono::system_clock::now();
		fmt::print("AudioEngine::startRecording::elapsed: recorded {} bytes ({}ms)\n",
			m_sampleBuffer.size(),
			std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
		);
	}

	fmt::print("AudioEngine::startRecording::end\n");
}

/*
// returns a vector of size numBuckets + 1, fist and last will be min/max above
std::vector<float> GLAudioVisApp::calculateBuckets(int numBuckets, float powerCurve)
{
	// set this according to human hearing ranges, or something
	constexpr float minBucketFreq = 20.0f;
	constexpr float maxBucketFreq = 20000.0f;

	std::vector<float> buckets(numBuckets + 1);
	buckets.front() = minBucketFreq;
	buckets.back() = maxBucketFreq;

	for (int i = 1; i < numBuckets; ++i)
	{
		float t = static_cast<float>(i) / static_cast<float>(numBuckets);
		// t = pow(t, powerCurve);
		buckets[i] = minBucketFreq + (maxBucketFreq - minBucketFreq) * t;
	}
	fmt::print("buckets: {}\n", buckets);

	// std::vector<float> logBuckets = buckets;
	std::for_each(buckets.begin(), buckets.end(), [](float& x){ x = log10(x); });
	fmt::print("log buckets: {}\n", buckets);


	return buckets;
}
*/

// ImGui Helper Functions
void AudioEngine::plotInputPCM(
	const Channel& channel,
	const char* label,
	const char* overlay,
	const ImVec2& size
)
{
	// Cast the start of the buffer to a float array, since that's what ImGui needs to plot lines, and sizeof(float)
	// matches m_bytesPerSample
	assert(sizeof(float) == pa_sample_size_of_format(m_samplingSettings.sampleFormat));
	const float* buf = reinterpret_cast<float*>(m_sampleBuffer.data());

	ImGui::PlotLines(
		label,
		&buf[static_cast<unsigned char>(channel)], // start index
		m_samplingSettings.numSamples / m_samplingSettings.numChannels,
		0,
		overlay,
		-1.0f,
		1.0f,
		size,
		sizeof(float) * m_samplingSettings.numChannels // stride
	);
}

void AudioEngine::plotDFT(
	const Channel& channel,
	const char* label,
	const char* overlay,
	const ImVec2& size
)
{
	ImGui::PlotLines(
		label,
		m_fftData[static_cast<unsigned char>(channel)].dftOutputRaw.data(),
		m_fftData[static_cast<unsigned char>(channel)].dftOutputRaw.size(),
		0,
		overlay,
		0.0f,
		48.0f,
		size
	);
}

void AudioEngine::plotSpectrum(
	const Channel& channel,
	const char* label,
	const char* overlay,
	const ImVec2& size
)
{
	ImGui::PlotHistogram(
		label,
		m_fftData[static_cast<unsigned char>(channel)].spectrumBuckets.data(),
		m_numSpectrumBuckets,
		0,
		overlay,
		0.0f,
		48.0f,
		size
	);
}

void AudioEngine::setSpectrumBucketCount(unsigned int bucketCount)
{
	m_numSpectrumBuckets = bucketCount;

	for (auto& fftData : m_fftData)
	{
		fftData.spectrumBuckets.clear();
		fftData.spectrumBuckets.resize(bucketCount);
	}
}

const std::vector<float> AudioEngine::getDFT(const Channel& channel) const
{
	return m_fftData[static_cast<unsigned char>(channel)].dftOutputRaw;
}
