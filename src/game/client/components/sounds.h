/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SOUNDS_H
#define GAME_CLIENT_COMPONENTS_SOUNDS_H

#include <engine/sound.h>
#include <game/client/component.h>
#include <engine/shared/jobs.h>

class CSounds : public CComponent
{
	enum
	{
		QUEUE_SIZE = 32,
	};
	struct QueueEntry
	{
		int m_Channel;
		int m_SetId;
	} m_aQueue[QUEUE_SIZE];
	int m_QueuePos;
	int64 m_QueueWaitTime;
	class CJob : public IJob
	{
		struct CUserData *m_pData;
	public:
		CJob(struct CUserData *pData) : m_pData(pData) {}

		void Run() override;
	};
	std::shared_ptr<CJob> m_pSoundJob;
	bool m_WaitForSoundJob;
	
	ISound::CSampleHandle GetSampleId(int SetId);

	friend class CSounds::CJob;
public:
	// sound channels
	enum
	{
		CHN_GUI=0,
		CHN_MUSIC,
		CHN_WORLD,
		CHN_GLOBAL,
	};

	int GetInitAmount() const override;
	void OnInit() override;
	void OnReset() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;

	void ClearQueue();
	void Enqueue(int Channel, int SetId);
	void Play(int Channel, int SetId, float Vol);
	void PlayAt(int Channel, int SetId, float Vol, vec2 Pos);
	void Stop(int SetId);
	bool IsPlaying(int SetId);
};


#endif
