/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_SOUND_H
#define ENGINE_CLIENT_SOUND_H

#include <engine/sound.h>

class CSound : public IEngineSound
{
	int m_SoundEnabled;

public:
	CConfig *m_pConfig;
	IEngineGraphics *m_pGraphics;
	IStorage *m_pStorage;

	int Init() override;

	int Update() override;
	void Shutdown() override;
	int AllocID();

	static void RateConvert(int SampleID);

	bool IsSoundEnabled() override { return m_SoundEnabled != 0; }

	CSampleHandle LoadWV(const char *pFilename) override;

	void SetListenerPos(float x, float y) override;
	void SetChannelVolume(int ChannelID, float Vol) override;
	void SetMaxDistance(float Distance) override;

	int Play(int ChannelID, CSampleHandle SampleID, int Flags, float x, float y);
	int PlayAt(int ChannelID, CSampleHandle SampleID, int Flags, float x, float y) override;
	int Play(int ChannelID, CSampleHandle SampleID, int Flags) override;
	void Stop(CSampleHandle SampleID) override;
	void StopAll() override;
	bool IsPlaying(CSampleHandle SampleID) override;
};

#endif
