#include "GLAudioVisApp.h"

// #include <pulse/error.h>
// #include <pulse/pulseaudio.h>
// #include <pulse/simple.h>

// #include <fmt/core.h>
// #include <fmt/ranges.h>

int main(int argc, char* argv[])
{
	return GLAudioVisApp::execute(argc, argv);
	/*
	// Specify the sample format, should be possible to determine this from `pacmd list-sources`?
	pa_sample_spec sampleFormat;
	sampleFormat.format = PA_SAMPLE_FLOAT32LE; // PA_SAMPLE_S16LE; // 2 byte sample
	sampleFormat.channels = 1;
	// sampleFormat.rate = 44100;
	sampleFormat.rate = 48000;

	// pa_buffer_attr ba;
	// ba.maxlength = 1024; // max length of the buffer
	// ba.tlength = (uint32_t)-1; // target buffer length (bytes) ?  playback only?
	// ba.minreq = 1024; // minimum request ?
	// ba.fragsize = 1024; // fragment size (bytes) recording only?

	int error;

	// const std::string source = "alsa_input.pci-0000_00_1b.0.analog-stereo";
	const std::string source = "alsa_output.pci-0000_00_1b.0.analog-stereo.monitor";

	// connect to the PulseAudio server
	pa_simple* audioSource = pa_simple_new(
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

	if (audioSource == nullptr)
	{
		fmt::print("pulseaudio failed to connect to audio source '{}', error: {}\n", source.c_str(), pa_strerror(error));
	}
	else
	{
		fmt::print("success!\n");
	}

	const size_t bytesPerSample = pa_sample_size(&sampleFormat);
	const size_t numFrames = 1024;
	const size_t bufferSize = bytesPerSample * numFrames; // bytes
	std::vector<char> buffer(bufferSize, {}); // use char here, as 1 byte

	fmt::print("bytes per sample: {}, buffer size: {}\n", bytesPerSample, bufferSize);

	// main loop
	for (int i = 0; i < 10; ++i)
	{
		if (pa_simple_read(audioSource, buffer.data(), bufferSize, &error) < 0)
		{
			fmt::print("pulseaudio failed to read: {}\n", pa_strerror(error));
			break;
		}




		char* ptr = buffer.data();
		for (unsigned int j = 0; j < numFrames; ++j)
		{
			const float sample = *reinterpret_cast<const float*>(ptr); // pcm to real conversion?
			// float realSample = float(sample) / 32768;
			// const float freq = float(sample * sampleFormat.rate) / float(numFrames);

			// fmt::print("[{}:{}] = {}\n", i, j, sample);
			fmt::print("[{}] = {}\n", j, sample);
			ptr += bytesPerSample;
		}
	}

	pa_simple_free(audioSource);

	return EXIT_SUCCESS;
	*/
}
