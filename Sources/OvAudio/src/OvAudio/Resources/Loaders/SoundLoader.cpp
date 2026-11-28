/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <limits>
#include <string>
#include <vector>

#include <soloud_wav.h>

#include <OvDebug/Logger.h>
#include <OvTools/Filesystem/VirtualFileSystem.h>

#include <OvAudio/Resources/Loaders/SoundLoader.h>

namespace
{
	// From a mounted package, or the disk. The samples are decoded at once, so the bytes are not kept
	SoLoud::result LoadWav(SoLoud::Wav& p_wav, const std::string& p_filepath)
	{
		std::vector<char> bytes;
		std::string error;
		if (!OvTools::Filesystem::VirtualFileSystem::Read(p_filepath, bytes, error) || bytes.empty() ||
			bytes.size() > std::numeric_limits<unsigned int>::max())
		{
			return SoLoud::FILE_NOT_FOUND;
		}

		return p_wav.loadMem(reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<unsigned int>(bytes.size()), false, false);
	}
}

OvAudio::Resources::Sound* OvAudio::Resources::Loaders::SoundLoader::Create(const std::string& p_filepath)
{
	std::unique_ptr<SoLoud::Wav> wav = std::make_unique<SoLoud::Wav>();

	// A missing or unreadable file gives no sound, rather than a sound that plays nothing
	if (LoadWav(*wav, p_filepath) != SoLoud::SO_NO_ERROR)
	{
		OVLOG_WARNING("Cannot read the sound \"" + p_filepath + "\": a WAV, OGG, MP3 or FLAC file was expected");
		return nullptr;
	}

	return new Sound(p_filepath, std::move(wav));
}

void OvAudio::Resources::Loaders::SoundLoader::Reload(Sound& p_sound, const std::string& p_path)
{
	*const_cast<std::string*>(&p_sound.path) = p_path;
	LoadWav(*p_sound.audioData, p_path);
}

bool OvAudio::Resources::Loaders::SoundLoader::Destroy(Sound*& p_soundInstance)
{
	if (p_soundInstance)
	{
		delete p_soundInstance;
		p_soundInstance = nullptr;

		return true;
	}

	return false;
}
