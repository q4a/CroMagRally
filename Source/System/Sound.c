/****************************/
/*     SOUND ROUTINES       */
/* By Brian Greenstone      */
/* (c)2000 Pangea Software  */
/* (c)2022 Iliyas Jorio     */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

#include "game.h"
#include <string.h>

/****************************/
/*    PROTOTYPES            */
/****************************/

static short FindSilentChannel(void);
static short EmergencyFreeChannel(void);
static void Calc3DEffectVolume(short effectNum, const OGLPoint3D *where, float volAdjust, uint32_t *leftVolOut, uint32_t *rightVolOut);


/****************************/
/*    CONSTANTS             */
/****************************/

#define	ANNOUNCER_VOLUME	(FULL_CHANNEL_VOLUME * 4)
#define	SONG_VOLUME		1.6f

#define FloatToFixed16(a)      ((Fixed)((float)(a) * 0x000100L))		// convert float to 16bit fixed pt


#define		MAX_CHANNELS			20

#define		MAX_EFFECTS				50

typedef struct
{
	Byte	bank;
	const char* filename;
	long	refDistance;
} EffectDef;

typedef struct
{
	SndListHandle	sndHandle;
	long			sndOffset;
	short			lastPlayedOnChannel;
	uint32_t		lastLoudness;
} LoadedEffect;

typedef struct
{
	uint16_t	effectNum;
	float	volumeAdjust;
	uint32_t	leftVolume, rightVolume;
}ChannelInfoType;

#define	VOLUME_DISTANCE_FACTOR	.001f		// bigger == sound decays FASTER with dist, smaller = louder far away

/**********************/
/*     VARIABLES      */
/**********************/

short						gRecentAnnouncerEffect = -1, gDelayedAnnouncerEffect;
short						gRecentAnnouncerChannel = -1;
float						gAnnouncerDelay = -1;

static float				gGlobalVolumeFade = 1.0f;
static float				gEffectsVolume = .4;
static float				gMusicVolume = .4;
static float				gMusicVolumeTweak = 1.0f;

OGLPoint3D					gEarCoords[MAX_LOCAL_PLAYERS];			// coord of camera plus a tad to get pt in front of camera
static	OGLVector3D			gEyeVector[MAX_LOCAL_PLAYERS];

static	LoadedEffect		gLoadedEffects[NUM_EFFECTS];

static	SndChannelPtr		gSndChannel[MAX_CHANNELS];
static	ChannelInfoType		gChannelInfo[MAX_CHANNELS];
static	SndChannelPtr		gMusicChannel;

static short				gMaxChannels = 0;

Boolean						gSongPlayingFlag = false;
Boolean						gLoopSongFlag = true;


Boolean				gMuteMusicFlag = false;
static short				gCurrentSong = -1;

Boolean						gFadeOutMusic = false;


		/*****************/
		/* EFFECTS TABLE */
		/*****************/


static const char* kSoundBankNames[NUM_SOUNDBANKS] =
{
	[SOUNDBANK_MAIN]			= "main",
	[SOUNDBANK_LEVELSPECIFIC]	= "levelspecific",
	[SOUNDBANK_ANNOUNCER]		= "announcer",
};

static const EffectDef kEffectsTable[NUM_EFFECTS] =
{
	[EFFECT_BADSELECT]          = {SOUNDBANK_MAIN,				"badselect",			2000},
	[EFFECT_SKID3]              = {SOUNDBANK_MAIN,				"skid3",				2000},
	[EFFECT_ENGINE]             = {SOUNDBANK_MAIN,				"engine",				1000},
	[EFFECT_SKID]               = {SOUNDBANK_MAIN,				"skid",					2000},
	[EFFECT_CRASH]              = {SOUNDBANK_MAIN,				"crash",				1000},
	[EFFECT_BOOM]               = {SOUNDBANK_MAIN,				"boom",					1500},
	[EFFECT_NITRO]              = {SOUNDBANK_MAIN,				"nitroburst",			5000},
	[EFFECT_ROMANCANDLE_LAUNCH] = {SOUNDBANK_MAIN,				"romancandlelaunch",	2000},
	[EFFECT_ROMANCANDLE_FALL]   = {SOUNDBANK_MAIN,				"romancandlefall",		4000},
	[EFFECT_SKID2]              = {SOUNDBANK_MAIN,				"skid2",				2000},
	[EFFECT_SELECTCLICK]        = {SOUNDBANK_MAIN,				"selectclick",			2000},
	[EFFECT_GETPOW]             = {SOUNDBANK_MAIN,				"getpow",				3000},
	[EFFECT_CANNON]             = {SOUNDBANK_MAIN,				"cannon",				2000},
	[EFFECT_CRASH2]             = {SOUNDBANK_MAIN,				"crash2",				3000},
	[EFFECT_SPLASH]             = {SOUNDBANK_MAIN,				"splash",				3000},
	[EFFECT_BIRDCAW]            = {SOUNDBANK_MAIN,				"birdcaw",				1000},
	[EFFECT_SNOWBALL]           = {SOUNDBANK_MAIN,				"snowball",				3000},
	[EFFECT_MINE]               = {SOUNDBANK_MAIN,				"dropmine",				1000},
	[EFFECT_THROW1]             = {SOUNDBANK_MAIN,				"throw1",				2000},
	[EFFECT_THROW2]             = {SOUNDBANK_MAIN,				"throw2",				2000},
	[EFFECT_THROW3]             = {SOUNDBANK_MAIN,				"throw3",				2000},
	[EFFECT_DUSTDEVIL]          = {SOUNDBANK_LEVELSPECIFIC,		"dustdevil",			2000},
	[EFFECT_BLOWDART]           = {SOUNDBANK_LEVELSPECIFIC,		"blowdart",				2000},
	[EFFECT_HITSNOW]            = {SOUNDBANK_LEVELSPECIFIC,		"hitsnow",				2000},
	[EFFECT_CATAPULT]           = {SOUNDBANK_LEVELSPECIFIC,		"catapult",				5000},
	[EFFECT_GONG]               = {SOUNDBANK_LEVELSPECIFIC,		"gong",					4000},
	[EFFECT_SHATTER]            = {SOUNDBANK_LEVELSPECIFIC,		"vaseshatter",			2000},
	[EFFECT_ZAP]                = {SOUNDBANK_LEVELSPECIFIC,		"zap",					4000},
	[EFFECT_TORPEDOFIRE]        = {SOUNDBANK_LEVELSPECIFIC,		"torpedofire",			3000},
	[EFFECT_HUM]                = {SOUNDBANK_LEVELSPECIFIC,		"hum",					2000},
	[EFFECT_BUBBLES]            = {SOUNDBANK_LEVELSPECIFIC,		"bubbles",				2500},
	[EFFECT_CHANT]              = {SOUNDBANK_LEVELSPECIFIC,		"chant",				3000},
	[EFFECT_READY]              = {SOUNDBANK_ANNOUNCER,			"ready",				2000},
	[EFFECT_SET]                = {SOUNDBANK_ANNOUNCER,			"set",					2000},
	[EFFECT_GO]                 = {SOUNDBANK_ANNOUNCER,			"go",					1000},
	[EFFECT_THATSALL]           = {SOUNDBANK_ANNOUNCER,			"thatsall",				2000},
	[EFFECT_GOODJOB]            = {SOUNDBANK_ANNOUNCER,			"goodjob",				2000},
	[EFFECT_REDTEAMWINS]        = {SOUNDBANK_ANNOUNCER,			"redteamwins",			4000},
	[EFFECT_GREENTEAMWINS]      = {SOUNDBANK_ANNOUNCER,			"greenteamwins",		4000},
	[EFFECT_LAP2]               = {SOUNDBANK_ANNOUNCER,			"lap2",					1000},
	[EFFECT_FINALLAP]           = {SOUNDBANK_ANNOUNCER,			"finallap",				1000},
	[EFFECT_NICESHOT]           = {SOUNDBANK_ANNOUNCER,			"niceshot",				1000},
	[EFFECT_GOTTAHURT]          = {SOUNDBANK_ANNOUNCER,			"gottahurt",			1000},
	[EFFECT_1st]                = {SOUNDBANK_ANNOUNCER,			"1st",					1000},
	[EFFECT_2nd]                = {SOUNDBANK_ANNOUNCER,			"2nd",					1000},
	[EFFECT_3rd]                = {SOUNDBANK_ANNOUNCER,			"3rd",					1000},
	[EFFECT_4th]                = {SOUNDBANK_ANNOUNCER,			"4th",					1000},
	[EFFECT_5th]                = {SOUNDBANK_ANNOUNCER,			"5th",					1000},
	[EFFECT_6th]                = {SOUNDBANK_ANNOUNCER,			"6th",					1000},
	[EFFECT_OHYEAH]             = {SOUNDBANK_ANNOUNCER,			"ohyeah",				1000},
	[EFFECT_NICEDRIVING]        = {SOUNDBANK_ANNOUNCER,			"nicedrivin",			1000},
	[EFFECT_WOAH]               = {SOUNDBANK_ANNOUNCER,			"woah",					1000},
	[EFFECT_YOUWIN]             = {SOUNDBANK_ANNOUNCER,			"youwin",				1000},
	[EFFECT_YOULOSE]            = {SOUNDBANK_ANNOUNCER,			"youlose",				1000},
	[EFFECT_POW_BONEBOMB]       = {SOUNDBANK_ANNOUNCER,			"bonebomb",				1000},
	[EFFECT_POW_OIL]            = {SOUNDBANK_ANNOUNCER,			"oil",					1000},
	[EFFECT_POW_NITRO]          = {SOUNDBANK_ANNOUNCER,			"nitro",				1000},
	[EFFECT_POW_PIGEON]         = {SOUNDBANK_ANNOUNCER,			"pigeon",				1000},
	[EFFECT_POW_CANDLE]         = {SOUNDBANK_ANNOUNCER,			"candle",				1000},
	[EFFECT_POW_ROCKET]         = {SOUNDBANK_ANNOUNCER,			"rocket",				1000},
	[EFFECT_POW_TORPEDO]        = {SOUNDBANK_ANNOUNCER,			"torpedo",				1000},
	[EFFECT_POW_FREEZE]         = {SOUNDBANK_ANNOUNCER,			"freeze",				1000},
	[EFFECT_POW_MINE]           = {SOUNDBANK_ANNOUNCER,			"mine",					1000},
	[EFFECT_POW_SUSPENSION]     = {SOUNDBANK_ANNOUNCER,			"suspension",			1000},
	[EFFECT_POW_INVISIBILITY]   = {SOUNDBANK_ANNOUNCER,			"invisibility",			1000},
	[EFFECT_POW_STICKYTIRES]    = {SOUNDBANK_ANNOUNCER,			"stickytires",			1000},
	[EFFECT_ARROWHEAD]          = {SOUNDBANK_ANNOUNCER,			"arrowhead",			1000},
	[EFFECT_COMPLETED]          = {SOUNDBANK_ANNOUNCER,			"completed",			1000},
	[EFFECT_INCOMPLETE]         = {SOUNDBANK_ANNOUNCER,			"incomplete",			1000},
	[EFFECT_COSTYA]             = {SOUNDBANK_ANNOUNCER,			"costya",				1000},
	[EFFECT_WATCHIT]            = {SOUNDBANK_ANNOUNCER,			"watchit",				1000},
};


/********************* INIT SOUND TOOLS ********************/

void InitSoundTools(void)
{
OSErr			iErr;

	gRecentAnnouncerEffect = -1;
	gRecentAnnouncerChannel = -1;
	gAnnouncerDelay = 0;

	gMaxChannels = 0;

			/* INIT BANK INFO */

	memset(gLoadedEffects, 0, sizeof(gLoadedEffects));

			/******************/
			/* ALLOC CHANNELS */
			/******************/

			/* ALL OTHER CHANNELS */

	for (gMaxChannels = 0; gMaxChannels < MAX_CHANNELS; gMaxChannels++)
	{
			/* NEW SOUND CHANNEL */

		iErr = SndNewChannel(&gSndChannel[gMaxChannels],sampledSynth,0,nil);
		if (iErr)												// if err, stop allocating channels
			break;
	}


	iErr = SndNewChannel(&gMusicChannel, sampledSynth, 0, nil);
	GAME_ASSERT_MESSAGE(!iErr, "Couldn't allocate music channel");


		/* LOAD DEFAULT SOUNDS */

	LoadSoundBank(SOUNDBANK_MAIN);

	UpdateGlobalVolume();
}


/******************** SHUTDOWN SOUND ***************************/
//
// Called at Quit time
//

void ShutdownSound(void)
{
int	i;

			/* STOP ANY PLAYING AUDIO */

	StopAllEffectChannels();
	KillSong();

	DisposeAllSoundBanks();


		/* DISPOSE OF CHANNELS */

	for (i = 0; i < gMaxChannels; i++)
		SndDisposeChannel(gSndChannel[i], true);
	gMaxChannels = 0;


}

#pragma mark -


/******************* LOAD A SOUND EFFECT ************************/

void LoadSoundEffect(int effectNum)
{
char path[256];
FSSpec spec;
short refNum;
OSErr err;

	LoadedEffect* loadedSound = &gLoadedEffects[effectNum];
	const EffectDef* effectDef = &kEffectsTable[effectNum];

	if (loadedSound->sndHandle)
	{
		// already loaded
		return;
	}

	snprintf(path, sizeof(path), ":audio:%s:%s.aiff", kSoundBankNames[effectDef->bank], effectDef->filename);

	err = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, path, &spec);
	if (err != noErr)
	{
		DoAlert(path);
		return;
	}

	err = FSpOpenDF(&spec, fsRdPerm, &refNum);
	GAME_ASSERT_MESSAGE(err == noErr, path);

	loadedSound->sndHandle = Pomme_SndLoadFileAsResource(refNum);
	GAME_ASSERT_MESSAGE(loadedSound->sndHandle, path);

	FSClose(refNum);

			/* GET OFFSET INTO IT */

	GetSoundHeaderOffset(loadedSound->sndHandle, &loadedSound->sndOffset);

			/* PRE-DECOMPRESS IT */

	Pomme_DecompressSoundResource(&loadedSound->sndHandle, &loadedSound->sndOffset);
}

/******************* DISPOSE OF A SOUND EFFECT ************************/

void DisposeSoundEffect(int effectNum)
{
	LoadedEffect* loadedSound = &gLoadedEffects[effectNum];

	if (loadedSound->sndHandle)
	{
		DisposeHandle((Handle) loadedSound->sndHandle);
		memset(loadedSound, 0, sizeof(LoadedEffect));
	}
}

/******************* LOAD SOUND BANK ************************/

void LoadSoundBank(int bankNum)
{
	StopAllEffectChannels();

			/****************************/
			/* LOAD ALL EFFECTS IN BANK */
			/****************************/

	for (int i = 0; i < NUM_EFFECTS; i++)
	{
		if (kEffectsTable[i].bank == bankNum)
		{
			LoadSoundEffect(i);
		}
	}
}

/******************** DISPOSE SOUND BANK **************************/

void DisposeSoundBank(int bankNum)
{
	gAnnouncerDelay = 0;										// set this to avoid announcer talking after the sound bank is gone

	StopAllEffectChannels();									// make sure all sounds are stopped before nuking any banks

			/* FREE ALL SAMPLES */

	for (int i = 0; i < NUM_EFFECTS; i++)
	{
		if (kEffectsTable[i].bank == bankNum)
		{
			DisposeSoundEffect(i);
		}
	}
}


/******************* DISPOSE ALL SOUND BANKS *****************/

void DisposeAllSoundBanks(void)
{
	for (int i = 0; i < NUM_SOUNDBANKS; i++)
	{
		DisposeSoundBank(i);
	}
}


/********************* STOP A CHANNEL **********************/
//
// Stops the indicated sound channel from playing.
//

void StopAChannel(short *channelNum)
{
SndCommand 	mySndCmd;
SCStatus	theStatus = {0};
short		c = *channelNum;

	if ((c < 0) || (c >= gMaxChannels))		// make sure its a legal #
		return;

	SndChannelStatus(gSndChannel[c],sizeof(SCStatus),&theStatus);	// get channel info
	if (theStatus.scChannelBusy)					// if channel busy, then stop it
	{

		mySndCmd.cmd = flushCmd;
		mySndCmd.param1 = 0;
		mySndCmd.param2 = 0;
		SndDoImmediate(gSndChannel[c], &mySndCmd);

		mySndCmd.cmd = quietCmd;
		mySndCmd.param1 = 0;
		mySndCmd.param2 = 0;
		SndDoImmediate(gSndChannel[c], &mySndCmd);
	}

	*channelNum = -1;
}


/********************* STOP A CHANNEL IF EFFECT NUM **********************/
//
// Stops the indicated sound channel from playing if it is still this effect #
//

void StopAChannelIfEffectNum(short *channelNum, short effectNum)
{
SndCommand 	mySndCmd;
SCStatus	theStatus = {0};
short		c = *channelNum;

	if ((c < 0) || (c >= gMaxChannels))				// make sure its a legal #
		return;

	if (gChannelInfo[c].effectNum != effectNum)		// make sure its the right effect
		return;


	SndChannelStatus(gSndChannel[c],sizeof(SCStatus),&theStatus);	// get channel info
	if (theStatus.scChannelBusy)					// if channel busy, then stop it
	{
		mySndCmd.cmd = flushCmd;
		mySndCmd.param1 = 0;
		mySndCmd.param2 = 0;
		SndDoImmediate(gSndChannel[c], &mySndCmd);

		mySndCmd.cmd = quietCmd;
		mySndCmd.param1 = 0;
		mySndCmd.param2 = 0;
		SndDoImmediate(gSndChannel[c], &mySndCmd);
	}

	*channelNum = -1;
}



/********************* STOP ALL EFFECT CHANNELS **********************/

void StopAllEffectChannels(void)
{
short		i;

	for (i=0; i < gMaxChannels; i++)
	{
		short	c;

		c = i;
		StopAChannel(&c);
	}
}


/****************** WAIT EFFECTS SILENT *********************/

void WaitEffectsSilent(void)
{
short	i;
Boolean	isBusy;
SCStatus				theStatus;

	do
	{
		isBusy = 0;
		for (i=0; i < gMaxChannels; i++)
		{
			SndChannelStatus(gSndChannel[i],sizeof(SCStatus),&theStatus);	// get channel info
			isBusy |= theStatus.scChannelBusy;
		}
	}while(isBusy);
}

#pragma mark -

/******************** PLAY SONG ***********************/
//
// if songNum == -1, then play existing open song
//
// INPUT: loopFlag = true if want song to loop
//

void PlaySong(short songNum, Boolean loopFlag)
{
OSErr 	iErr;
static	SndCommand 		mySndCmd;
FSSpec	spec;
short	musicFileRefNum;

	if (songNum == gCurrentSong)					// see if this is already playing
		return;


		/* ZAP ANY EXISTING SONG */

	gCurrentSong 	= songNum;
	gLoopSongFlag 	= loopFlag;
	KillSong();
	DoSoundMaintenance();

		/* ENFORCE MUTE PREF */

	gMuteMusicFlag = !gGamePrefs.music;

			/******************************/
			/* OPEN APPROPRIATE AIFF FILE */
			/******************************/

	static const struct
	{
		const char* path;
		float volumeTweak;
	} songs[MAX_SONGS] =
	{
		[SONG_WIN]		= {":audio:WinSong.aiff", 1.9f},
		[SONG_DESERT]	= {":audio:DesertSong.aiff", 1.0f},
		[SONG_JUNGLE]	= {":audio:JungleSong.aiff", 1.5f},
		[SONG_THEME]	= {":audio:ThemeSong.aiff", 1.5f},
		[SONG_ATLANTIS]	= {":audio:AtlantisSong.aiff", 1.5f},
		[SONG_CHINA]	= {":audio:ChinaSong.aiff", 1.5f},
		[SONG_CRETE]	= {":audio:CreteSong.aiff", 1.5f},
		[SONG_EUROPE]	= {":audio:EuroSong.aiff", 1.5f},
		[SONG_ICE]		= {":audio:IceSong.aiff", 1.5f},
		[SONG_EGYPT]	= {":audio:EgyptSong.aiff", 1.5f},
		[SONG_VIKING]	= {":audio:VikingSong.aiff", 1.5f},
	};

	if (songNum < 0 || songNum >= MAX_SONGS)
		DoFatalAlert("PlaySong: unknown song #");

	iErr = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, songs[songNum].path, &spec);
	GAME_ASSERT(!iErr);

	iErr = FSpOpenDF(&spec, fsRdPerm, &musicFileRefNum);
	GAME_ASSERT(!iErr);

	gMusicVolumeTweak = songs[songNum].volumeTweak;

	gCurrentSong = songNum;


				/*****************/
				/* START PLAYING */
				/*****************/

			/* START PLAYING FROM FILE */

	iErr = SndStartFilePlay(
		gMusicChannel,
		musicFileRefNum,
		0,
		0, //STREAM_BUFFER_SIZE
		0, //gMusicBuffer
		nil,
		nil, //SongCompletionProc
		true);

	FSClose(musicFileRefNum);		// close the file (Pomme decompresses entire song into memory)

	if (iErr)
	{
		DoAlert("PlaySong: SndStartFilePlay failed!");
		ShowSystemErr(iErr);
	}
	gSongPlayingFlag = true;


			/* SET LOOP FLAG ON STREAM (SOURCE PORT ADDITION) */
			/* So we don't need to re-read the file over and over. */

	mySndCmd.cmd = pommeSetLoopCmd;
	mySndCmd.param1 = loopFlag ? 1 : 0;
	mySndCmd.param2 = 0;
	iErr = SndDoImmediate(gMusicChannel, &mySndCmd);
	if (iErr)
		DoFatalAlert("PlaySong: SndDoImmediate (pomme loop extension) failed!");

	uint32_t lv2 = kFullVolume * gMusicVolumeTweak * gMusicVolume;
	uint32_t rv2 = kFullVolume * gMusicVolumeTweak * gMusicVolume;
	mySndCmd.cmd = volumeCmd;
	mySndCmd.param1 = 0;
	mySndCmd.param2 = (rv2<<16) | lv2;
	iErr = SndDoImmediate(gMusicChannel, &mySndCmd);
	if (iErr)
		DoFatalAlert("PlaySong: SndDoImmediate (volumeCmd) failed!");





			/* SEE IF WANT TO MUTE THE MUSIC */

	if (gMuteMusicFlag)
		SndPauseFilePlay(gMusicChannel);						// pause it	

}



/*********************** KILL SONG *********************/

void KillSong(void)
{

	gCurrentSong = -1;

	if (!gSongPlayingFlag)
		return;

	gSongPlayingFlag = false;											// tell callback to do nothing

	SndStopFilePlay(gMusicChannel, true);								// stop it
}

/******************** TOGGLE MUSIC *********************/

void ToggleMusic(void)
{
	gMuteMusicFlag = !gMuteMusicFlag;

	if (gSongPlayingFlag)
	{
		SndPauseFilePlay(gMusicChannel);			// pause it
	}
}



#pragma mark -

/***************************** PLAY EFFECT 3D ***************************/
//
// NO SSP
//
// OUTPUT: channel # used to play sound
//

short PlayEffect3D(short effectNum, const OGLPoint3D *where)
{
short					theChan;
uint32_t				leftVol, rightVol;

				/* CALC VOLUME */

	Calc3DEffectVolume(effectNum, where, 1.0, &leftVol, &rightVol);
	if ((leftVol+rightVol) == 0)
		return(-1);


	theChan = PlayEffect_Parms(effectNum, leftVol, rightVol, NORMAL_CHANNEL_RATE);

	if (theChan != -1)
		gChannelInfo[theChan].volumeAdjust = 1.0;			// full volume adjust

	return(theChan);									// return channel #
}



/***************************** PLAY EFFECT PARMS 3D ***************************/
//
// Plays an effect with parameters in 3D
//
// OUTPUT: channel # used to play sound
//

short PlayEffect_Parms3D(short effectNum, const OGLPoint3D *where, uint32_t rateMultiplier, float volumeAdjust)
{
short			theChan;
uint32_t		leftVol, rightVol;

				/* CALC VOLUME */

	Calc3DEffectVolume(effectNum, where, volumeAdjust, &leftVol, &rightVol);
	if ((leftVol+rightVol) == 0)
		return(-1);


				/* PLAY EFFECT */

	theChan = PlayEffect_Parms(effectNum, leftVol, rightVol, rateMultiplier);

	if (theChan != -1)
		gChannelInfo[theChan].volumeAdjust = volumeAdjust;	// remember volume adjuster

	return(theChan);									// return channel #
}


/************************* UPDATE 3D SOUND CHANNEL ***********************/

void Update3DSoundChannel(short effectNum, short *channel, const OGLPoint3D *where)
{
SCStatus		theStatus;
uint32_t			leftVol,rightVol;
short			c;

	c = *channel;

	if (c == -1)
		return;


			/* SEE IF SOUND HAS COMPLETED */

	SndChannelStatus(gSndChannel[c],sizeof(SCStatus),&theStatus);	// get channel info
	if (!theStatus.scChannelBusy)									// see if channel not busy
	{
gone:
		*channel = -1;												// this channel is now invalid
		return;
	}

			/* MAKE SURE THE SAME SOUND IS STILL ON THIS CHANNEL */

	if (effectNum != gChannelInfo[c].effectNum)
		goto gone;


			/* UPDATE THE THING */

	Calc3DEffectVolume(gChannelInfo[c].effectNum, where, gChannelInfo[c].volumeAdjust, &leftVol, &rightVol);
	if ((leftVol+rightVol) == 0)										// if volume goes to 0, then kill channel
	{
		StopAChannel(channel);
		return;
	}

	ChangeChannelVolume(c, leftVol, rightVol);
}

/******************** CALC 3D EFFECT VOLUME *********************/

static void Calc3DEffectVolume(short effectNum, const OGLPoint3D *where, float volAdjust, uint32_t *leftVolOut, uint32_t *rightVolOut)
{
float	dist;
float	refDist,volumeFactor;
uint32_t	volume,left,right;
uint32_t	maxLeft,maxRight;

	dist 	= OGLPoint3D_Distance(where, &gEarCoords[0]);		// calc dist to sound for pane 0
	if (gNumSplitScreenPanes > 1)								// see if other pane is closer (thus louder)
	{
		float	dist2 = OGLPoint3D_Distance(where, &gEarCoords[1]);

		if (dist2 < dist)
			dist = dist2;
	}

			/* DO VOLUME CALCS */

	refDist = kEffectsTable[effectNum].refDistance;			// get ref dist

	dist -= refDist;
	if (dist <= 0.0f)
		volumeFactor = 1.0f;
	else
	{
		volumeFactor = 1.0f / (dist * VOLUME_DISTANCE_FACTOR);
		if (volumeFactor > 1.0f)
			volumeFactor = 1.0f;
	}

	volume = (float)FULL_CHANNEL_VOLUME * volumeFactor * volAdjust;


	if (volume < 6)							// if really quiet, then just turn it off
	{
		*leftVolOut = *rightVolOut = 0;
		return;
	}

			/************************/
			/* DO STEREO SEPARATION */
			/************************/

	else
	{
		float		volF = (float)volume;
		OGLVector2D	earToSound,lookVec;
		int			i;
		float		dot,cross;

		maxLeft = maxRight = 0;

		for (i = 0; i < gNumSplitScreenPanes; i++)										// calc for each camera and use max of left & right
		{

				/* CALC VECTOR TO SOUND */

			earToSound.x = where->x - gEarCoords[0].x;
			earToSound.y = where->z - gEarCoords[0].z;
			FastNormalizeVector2D(earToSound.x, earToSound.y, &earToSound, true);


				/* CALC EYE LOOK VECTOR */

			FastNormalizeVector2D(gEyeVector[0].x, gEyeVector[0].z, &lookVec, true);


				/* DOT PRODUCT  TELLS US HOW MUCH STEREO SHIFT */

			dot = 1.0f - fabs(OGLVector2D_Dot(&earToSound,  &lookVec));
			if (dot < 0.0f)
				dot = 0.0f;
			else
			if (dot > 1.0f)
				dot = 1.0f;


				/* CROSS PRODUCT TELLS US WHICH SIDE */

			cross = OGLVector2D_Cross(&earToSound,  &lookVec);


					/* DO LEFT/RIGHT CALC */

			if (cross > 0.0f)
			{
				left 	= volF + (volF * dot);
				right 	= volF - (volF * dot);
			}
			else
			{
				right 	= volF + (volF * dot);
				left 	= volF - (volF * dot);
			}


					/* KEEP MAX */

			if (left > maxLeft)
				maxLeft = left;
			if (right > maxRight)
				maxRight = right;

		}


	}

	*leftVolOut = maxLeft;
	*rightVolOut = maxRight;
}



#pragma mark -

/******************* UPDATE LISTENER LOCATION ******************/
//
// Get ear coord for all local players
//

void UpdateListenerLocation(OGLSetupOutputType *setupInfo)
{
OGLVector3D	v;
int			i;

	for (i = 0; i < gNumSplitScreenPanes; i++)
	{
		v.x = setupInfo->cameraPlacement[i].pointOfInterest.x - setupInfo->cameraPlacement[i].cameraLocation.x;	// calc line of sight vector
		v.y = setupInfo->cameraPlacement[i].pointOfInterest.y - setupInfo->cameraPlacement[i].cameraLocation.y;
		v.z = setupInfo->cameraPlacement[i].pointOfInterest.z - setupInfo->cameraPlacement[i].cameraLocation.z;
		FastNormalizeVector(v.x, v.y, v.z, &v);

		gEarCoords[i].x = setupInfo->cameraPlacement[i].cameraLocation.x + (v.x * 300.0f);			// put ear coord in front of camera
		gEarCoords[i].y = setupInfo->cameraPlacement[i].cameraLocation.y + (v.y * 300.0f);
		gEarCoords[i].z = setupInfo->cameraPlacement[i].cameraLocation.z + (v.z * 300.0f);

		gEyeVector[i] = v;
	}
}


/***************************** PLAY EFFECT ***************************/
//
// OUTPUT: channel # used to play sound
//

short PlayEffect(short effectNum)
{
	return(PlayEffect_Parms(effectNum,FULL_CHANNEL_VOLUME,FULL_CHANNEL_VOLUME,NORMAL_CHANNEL_RATE));

}

/***************************** PLAY EFFECT PARMS ***************************/
//
// Plays an effect with parameters
//
// OUTPUT: channel # used to play sound
//

short  PlayEffect_Parms(short effectNum, uint32_t leftVolume, uint32_t rightVolume, unsigned long rateMultiplier)
{
SndCommand 		mySndCmd;
SndChannelPtr	chanPtr;
short			theChan;
OSErr			myErr;
uint32_t		lv2,rv2;
//static UInt32          loopStart, loopEnd;


			/* GET BANK & SOUND #'S FROM TABLE */

	LoadedEffect* sound = &gLoadedEffects[effectNum];

	GAME_ASSERT_MESSAGE(effectNum >= 0 && effectNum < NUM_EFFECTS, "illegal effect number");
	GAME_ASSERT_MESSAGE(sound->sndHandle, "effect wasn't loaded!");

			/* LOOK FOR FREE CHANNEL */

	theChan = FindSilentChannel();
	if (theChan == -1)
	{
		return(-1);
	}

	lv2 = (float)leftVolume * gEffectsVolume;							// amplify by global volume
	rv2 = (float)rightVolume * gEffectsVolume;


					/* GET IT GOING */

	chanPtr = gSndChannel[theChan];

	mySndCmd.cmd = flushCmd;
	mySndCmd.param1 = 0;
	mySndCmd.param2 = 0;
	myErr = SndDoImmediate(chanPtr, &mySndCmd);
	if (myErr)
		return(-1);

	mySndCmd.cmd = quietCmd;
	mySndCmd.param1 = 0;
	mySndCmd.param2 = 0;
	myErr = SndDoImmediate(chanPtr, &mySndCmd);
	if (myErr)
		return(-1);

	mySndCmd.cmd = volumeCmd;										// set sound playback volume
	mySndCmd.param1 = 0;
	mySndCmd.param2 = (rv2<<16) | lv2;
	myErr = SndDoImmediate(chanPtr, &mySndCmd);


	mySndCmd.cmd = bufferCmd;										// make it play
	mySndCmd.param1 = 0;
	mySndCmd.ptr = ((Ptr) *sound->sndHandle) + sound->sndOffset;	// pointer to SoundHeader
    SndDoImmediate(chanPtr, &mySndCmd);
	if (myErr)
		return(-1);

	mySndCmd.cmd 		= rateMultiplierCmd;						// modify the rate to change the frequency
	mySndCmd.param1 	= 0;
	mySndCmd.param2 	= rateMultiplier;
	SndDoImmediate(chanPtr, &mySndCmd);

    // If the loop start point is before the loop end, then there is a loop
	/*
    sndPtr = (SoundHeaderPtr)(((long)*gSndHandles[bankNum][soundNum])+gSndOffsets[bankNum][soundNum]);
    loopStart = sndPtr->loopStart;
    loopEnd = sndPtr->loopEnd;
    if (loopStart + 1 < loopEnd)
    {
    	mySndCmd.cmd = callBackCmd;										// let us know when the buffer is done playing
    	mySndCmd.param1 = 0;
    	mySndCmd.param2 = ((long)*gSndHandles[bankNum][soundNum])+gSndOffsets[bankNum][soundNum];	// pointer to SoundHeader
    	SndDoCommand(chanPtr, &mySndCmd, true);
	}
	*/


			/* SET MY INFO */

	gChannelInfo[theChan].effectNum 	= effectNum;		// remember what effect is playing on this channel
	gChannelInfo[theChan].leftVolume 	= leftVolume;		// remember requested volume (not the adjusted volume!)
	gChannelInfo[theChan].rightVolume 	= rightVolume;
	return(theChan);										// return channel #
}


#pragma mark -


/****************** UPDATE GLOBAL VOLUME ************************/
//
// Call this whenever gEffectsVolume is changed.  This will update
// all of the sounds with the correct volume.
//

void UpdateGlobalVolume(void)
{
	gEffectsVolume = 0.25f * (0.01f * gGamePrefs.sfxVolumePercent) * gGlobalVolumeFade;

			/* ADJUST VOLUMES OF ALL CHANNELS REGARDLESS IF THEY ARE PLAYING OR NOT */

	for (int c = 0; c < gMaxChannels; c++)
	{
		ChangeChannelVolume(c, gChannelInfo[c].leftVolume, gChannelInfo[c].rightVolume);
	}


			/* UPDATE SONG VOLUME */

	gMusicVolume = 0.4f * (0.01f * gGamePrefs.musicVolumePercent) * gGlobalVolumeFade;
	uint32_t lv2 = kFullVolume * gMusicVolumeTweak * gMusicVolume;
	uint32_t rv2 = kFullVolume * gMusicVolumeTweak * gMusicVolume;
	SndCommand cmd =
			{
			.cmd = volumeCmd,
			.param1 = 0,
			.param2 = (rv2<<16) | lv2,
	};
	SndDoImmediate(gMusicChannel, &cmd);

}

/*************** CHANGE CHANNEL VOLUME **************/
//
// Modifies the volume of a currently playing channel
//

void ChangeChannelVolume(short channel, uint32_t leftVol, uint32_t rightVol)
{
SndCommand 		mySndCmd;
SndChannelPtr	chanPtr;
uint32_t			lv2,rv2;

	if (channel < 0)									// make sure it's valid
		return;

	lv2 = (float)leftVol * gEffectsVolume;				// amplify by global volume
	rv2 = (float)rightVol * gEffectsVolume;

	chanPtr = gSndChannel[channel];						// get the actual channel ptr

	mySndCmd.cmd = volumeCmd;							// set sound playback volume
	mySndCmd.param1 = 0;
	mySndCmd.param2 = (rv2<<16) | lv2;			// set volume left & right
	SndDoImmediate(chanPtr, &mySndCmd);

	gChannelInfo[channel].leftVolume = leftVol;				// remember requested volume (not the adjusted volume!)
	gChannelInfo[channel].rightVolume = rightVol;
}



/*************** CHANGE CHANNEL RATE **************/
//
// Modifies the frequency of a currently playing channel
//
// The Input Freq is a fixed-point multiplier, not the static rate via rateCmd.
// This function uses rateMultiplierCmd, so a value of 0x00020000 is x2.0
//

void ChangeChannelRate(short channel, long rateMult)
{
static	SndCommand 		mySndCmd;
static	SndChannelPtr	chanPtr;

	if (channel < 0)									// make sure it's valid
		return;

	chanPtr = gSndChannel[channel];						// get the actual channel ptr

	mySndCmd.cmd 		= rateMultiplierCmd;						// modify the rate to change the frequency
	mySndCmd.param1 	= 0;
	mySndCmd.param2 	= rateMult;
	SndDoImmediate(chanPtr, &mySndCmd);
}




#pragma mark -


/******************** DO SOUND MAINTENANCE *************/
//
// 		ReadKeyboard() must have already been called
//

void DoSoundMaintenance(void)
{
				/* SEE IF TOGGLE MUSIC */

	if (GetNewNeedStateAnyP(kNeed_ToggleMusic))
	{
		ToggleMusic();
	}


#if 0
			/* SEE IF CHANGE VOLUME */

	if (GetNewKeyState_Real(kKey_RaiseVolume))
	{
		gEffectsVolume += .5f * gFramesPerSecondFrac;
		UpdateGlobalVolume();
	}
	else
	if (GetNewKeyState_Real(kKey_LowerVolume))
	{
		gEffectsVolume -= .5f * gFramesPerSecondFrac;
		if (gEffectsVolume < 0.0f)
			gEffectsVolume = 0.0f;
		UpdateGlobalVolume();
	}


				/* UPDATE SONG */

	if (gSongPlayingFlag)
	{
		if (IsMovieDone(gSongMovie))				// see if the song has completed
		{
			if (gLoopSongFlag)						// see if repeat it
			{
				GoToBeginningOfMovie(gSongMovie);
				StartMovie(gSongMovie);
			}
			else									// otherwise kill the song
				KillSong();
		}
		else
		{
			gMoviesTaskTimer -= gFramesPerSecondFrac;
			if (gMoviesTaskTimer <= 0.0f)
			{
				MoviesTask(gSongMovie, 0);
				gMoviesTaskTimer += .15f;
			}
		}
	}
#endif


#if 0
		/* WHILE WE'RE HERE, SEE IF SCALE SCREEN */

	if (GetNewKeyState_Real(KEY_F10))
	{
		gGamePrefs.screenCrop -= .1f;
		if (gGamePrefs.screenCrop < 0.0f)
			gGamePrefs.screenCrop = 0;
		ChangeWindowScale();
	}
	else
	if (GetNewKeyState_Real(KEY_F9))
	{
		gGamePrefs.screenCrop += .1f;
		if (gGamePrefs.screenCrop > 1.0f)
			gGamePrefs.screenCrop = 1.0;
		ChangeWindowScale();
	}


		/* ALSO CHECK OPTIONS */


	if (!gNetGameInProgress)
	{
		if (GetNewKeyState_Real(KEY_F1) && (!gIsSelfRunningDemo))
		{
			DoGameSettingsDialog();
		}
	}
#endif


		/* CHECK FOR ANNOUNCER */

	if (gAnnouncerDelay > 0.0f)
	{
		gAnnouncerDelay -= gFramesPerSecondFrac;
		if (gAnnouncerDelay <= 0.0f)								// see if ready to go
		{
			gAnnouncerDelay = 0;
			PlayAnnouncerSound(gDelayedAnnouncerEffect, false, 0);	// play the delayed effect now
		}
	}

}



/******************** FIND SILENT CHANNEL *************************/

static short FindSilentChannel(void)
{
short		theChan;
OSErr		myErr;
SCStatus	theStatus;

	for (theChan=0; theChan < gMaxChannels; theChan++)
	{
		myErr = SndChannelStatus(gSndChannel[theChan],sizeof(SCStatus),&theStatus);	// get channel info
		if (myErr)
			continue;
		if (!theStatus.scChannelBusy)					// see if channel not busy
		{
			return(theChan);
		}
	}

			/* NO FREE CHANNELS */

	return(-1);
}


/********************** IS EFFECT CHANNEL PLAYING ********************/

Boolean IsEffectChannelPlaying(short chanNum)
{
SCStatus	theStatus;

	SndChannelStatus(gSndChannel[chanNum],sizeof(SCStatus),&theStatus);	// get channel info
	return (theStatus.scChannelBusy);
}

/***************** EMERGENCY FREE CHANNEL **************************/
//
// This is used by the music streamer when all channels are used.
// Since we must have a channel to play music, we arbitrarily kill some other channel.
//

static short EmergencyFreeChannel(void)
{
short	i,c;

	for (i = 0; i < gMaxChannels; i++)
	{
		c = i;
		StopAChannel(&c);
		return(i);
	}

		/* TOO BAD, GOTTA NUKE ONE OF THE STREAMING CHANNELS IT SEEMS */

	StopAChannel(0);
	return(0);
}


#pragma mark -

/********************** PLAY ANNOUNCER SOUND ***************************/

void PlayAnnouncerSound(short effectNum, Boolean override, float delay)
{
	gAnnouncerDelay = delay;


				/*************************************/
				/* IF NO DELAY, THEN TRY TO PLAY NOW */
				/*************************************/

	if (delay == 0.0f)
	{

				/* SEE IF ANNOUNCER IS STILL TALKING */

		if ((!override) && (gRecentAnnouncerChannel != -1))										// see if announcer has spoken yet
		{
			if (IsEffectChannelPlaying(gRecentAnnouncerChannel))								// see if that channel is playing
			{
				if (gChannelInfo[gRecentAnnouncerChannel].effectNum == gRecentAnnouncerEffect)	// is it playing the announcer from last time?
					return;
			}
		}


				/* PLAY THE NEW EFFECT */

		gRecentAnnouncerChannel = PlayEffect_Parms(effectNum, ANNOUNCER_VOLUME, ANNOUNCER_VOLUME, NORMAL_CHANNEL_RATE);
		gRecentAnnouncerEffect = effectNum;
	}

				/***************************/
				/* DELAY, SO DONT PLAY NOW */
				/***************************/
	else
	{
		gDelayedAnnouncerEffect = effectNum;

	}

}



#pragma mark -

/********************** GLOBAL VOLUME FADE ***************************/


void FadeSound(float loudness)
{
	gGlobalVolumeFade = loudness;
	UpdateGlobalVolume();
}
