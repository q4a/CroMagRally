/****************************/
/*      MISC ROUTINES       */
/* (c)1996-2000 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

#include <SDL.h>
#include <math.h>

#include	"globals.h"
#include	"misc.h"
#include	"window.h"
#include "file.h"
#include "objects.h"
#include "input.h"
#include "skeletonobj.h"
#include "terrain.h"
#include "bg3d.h"
#include "sprites.h"
#include "network.h"
#include "sound2.h"
#include "miscscreens.h"

extern	unsigned long gOriginalSystemVolume;
extern	short		gMainAppRezFile;
extern	Boolean		gISPInitialized,gOSX;
extern	Boolean		gISpActive,gHasFixedDSpForX;
extern	/*DSpContextReference*/ void* 	gDisplayContext;
extern	OGLSetupOutputType		*gGameViewInfoPtr;
extern	int			gPolysThisFrame;
extern	float			gDemoVersionTimer;

extern	SDL_Window* 	gSDLWindow;


/****************************/
/*    CONSTANTS             */
/****************************/





#define		ERROR_ALERT_ID		401

#define	DEFAULT_FPS			9

/**********************/
/*     VARIABLES      */
/**********************/

Boolean	gQuitting = false;

short	gPrefsFolderVRefNum;
long	gPrefsFolderDirID;

u_long 	seed0 = 0, seed1 = 0, seed2 = 0;

float	gFramesPerSecond, gFramesPerSecondFrac;

int		gNumPointers = 0;

Boolean	gLowMemMode = false;


/**********************/
/*     PROTOTYPES     */
/**********************/



/*********************** DO ALERT *******************/

void DoAlert(const char* format, ...)
{
	GammaOn();
	Enter2D(true);

	char message[1024];
	va_list args;
	va_start(args, format);
	int result = vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	printf("CMR Alert: %s\n", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cro-Mag Rally", message, gSDLWindow);

	Exit2D();
	SDL_ShowCursor(0);
}


/*********************** DO FATAL ALERT *******************/

void DoFatalAlert(const char* format, ...)
{
	GammaOn();
	Enter2D(true);

	char message[1024];
	va_list args;
	va_start(args, format);
	int result = vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	printf("CMR Fatal Alert: %s\n", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cro-Mag Rally", message, NULL);//gSDLWindow);

	Exit2D();
	CleanQuit();
}


/************ CLEAN QUIT ***************/

void CleanQuit(void)
{
static Boolean	beenHere = false;

	gQuitting = true;

	if (!beenHere)
	{

#if DEMO || SHAREWARE
		DeleteAllObjects();
#endif

		beenHere = true;

		SavePlayerFile();								// save player if any

		EndNetworkGame();								// remove me from any active network game

		DisposeTerrain();								// dispose of any memory allocated by terrain manager
		DisposeAllBG3DContainers();						// nuke all models
		DisposeAllSpriteGroups();						// nuke all sprites


		if (gGameViewInfoPtr)							// see if need to dispose this
			OGL_DisposeWindowSetup(&gGameViewInfoPtr);


#if DEMO
		ShowDemoQuitScreen();
#elif SHAREWARE
		if (!gGameIsRegistered)
			DoSharewareExitScreen();
#endif

		ShutdownSound();								// cleanup sound stuff
	}


	GameScreenToBlack();
	CleanupDisplay();								// unloads Draw Sprocket
    TurnOnHighQualityRateConverter ();

#if 0
	if (gISPInitialized)							// unload ISp
		ISpShutdown();
#endif

	UseResFile(gMainAppRezFile);

#if 0
	InitCursor();
	FlushEvents ( everyEvent, REMOVE_ALL_EVENTS);
	FlushEventQueue(GetMainEventQueue());
#endif


	SavePrefs();							// save prefs before bailing

	ExitToShell();
}



#pragma mark -


/******************** MY RANDOM LONG **********************/
//
// My own random number generator that returns a LONG
//
// NOTE: call this instead of MyRandomShort if the value is going to be
//		masked or if it just doesnt matter since this version is quicker
//		without the 0xffff at the end.
//

unsigned long MyRandomLong(void)
{
  return seed2 ^= (((seed1 ^= (seed2>>5)*1568397607UL)>>7)+
                   (seed0 = (seed0+1)*3141592621UL))*2435386481UL;
}


/************************* RANDOM RANGE *************************/
//
// THE RANGE *IS* INCLUSIVE OF MIN AND MAX
//

u_short	RandomRange(unsigned short min, unsigned short max)
{
u_short		qdRdm;											// treat return value as 0-65536
u_long		range, t;

	qdRdm = MyRandomLong();
	range = max+1 - min;
	t = (qdRdm * range)>>16;	 							// now 0 <= t <= range

	return( t+min );
}



/************** RANDOM FLOAT ********************/
//
// returns a random float between 0 and 1
//

float RandomFloat(void)
{
unsigned long	r;
float	f;

	r = MyRandomLong() & 0xfff;
	if (r == 0)
		return(0);

	f = (float)r;							// convert to float
	f = f * (1.0f/(float)0xfff);			// get # between 0..1
	return(f);
}


/************** RANDOM FLOAT 2 ********************/
//
// returns a random float between 0 and 1
//

float RandomFloat2(void)
{
unsigned long	r;
float	f;

	r = MyRandomLong() & 0xfff;
	if (r == 0)
		return(0);

	f = (float)r;							// convert to float
	f = f * (2.0f/(float)0xfff);			// get # between 0..2
	f -= 1.0f;								// get -1..+1
	return(f);
}



/**************** SET MY RANDOM SEED *******************/

void SetMyRandomSeed(unsigned long seed)
{
	seed0 = seed;
	seed1 = 0;
	seed2 = 0;

}

/**************** INIT MY RANDOM SEED *******************/

void InitMyRandomSeed(void)
{
	seed0 = 0x2a80ce30;
	seed1 = 0;
	seed2 = 0;
}


#pragma mark -


/******************* FLOAT TO STRING *******************/

void FloatToString(float num, Str255 string)
{
Str255	sf;
long	i,f;

	i = num;						// get integer part


	f = (fabs(num)-fabs((float)i)) * 10000;		// reduce num to fraction only & move decimal --> 5 places

	if ((i==0) && (num < 0))		// special case if (-), but integer is 0
	{
		string[0] = 2;
		string[1] = '-';
		string[2] = '0';
	}
	else
		NumToString(i,string);		// make integer into string

	NumToString(f,sf);				// make fraction into string

	string[++string[0]] = '.';		// add "." into string

	if (f >= 1)
	{
		if (f < 1000)
			string[++string[0]] = '0';	// add 1000's zero
		if (f < 100)
			string[++string[0]] = '0';	// add 100's zero
		if (f < 10)
			string[++string[0]] = '0';	// add 10's zero
	}

	for (i = 0; i < sf[0]; i++)
	{
		string[++string[0]] = sf[i+1];	// copy fraction into string
	}
}

/*********************** STRING TO FLOAT *************************/

float StringToFloat(Str255 textStr)
{
short	i;
short	length;
Byte	mode = 0;
long	integer = 0;
long	mantissa = 0,mantissaSize = 0;
float	f;
float	tens[8] = {1,10,100,1000,10000,100000,1000000,10000000};
char	c;
float	sign = 1;												// assume positive

	length = textStr[0];										// get string length

	if (length== 0)												// quick check for empty
		return(0);


			/* SCAN THE NUMBER */

	for (i = 1; i <= length; i++)
	{
		c  = textStr[i];										// get this char

		if (c == '-')											// see if negative
		{
			sign = -1;
			continue;
		}
		else
		if (c == '.')											// see if hit the decimal
		{
			mode = 1;
			continue;
		}
		else
		if ((c < '0') || (c > '9'))								// skip all but #'s
			continue;


		if (mode == 0)
			integer = (integer * 10) + (c - '0');
		else
		{
			mantissa = (mantissa * 10) + (c - '0');
			mantissaSize++;
		}
	}

			/* BUILT A FLOAT FROM IT */

	f = (float)integer + ((float)mantissa/tens[mantissaSize]);
	f *= sign;

	return(f);
}





#pragma mark -

/****************** ALLOC HANDLE ********************/

Handle	AllocHandle(long size)
{
Handle	hand;
OSErr	err;

	hand = NewHandle(size);							// alloc in APPL
	if (hand == nil)
	{
		DoAlert("AllocHandle: using temp mem");
		hand = TempNewHandle(size,&err);			// try TEMP mem
		if (hand == nil)
		{
			DoAlert("AllocHandle: failed!");
			return(nil);
		}
		else
			return(hand);
	}
	return(hand);

}



/****************** ALLOC PTR ********************/

void *AllocPtr(long size)
{
Ptr	pr;
u_long	*cookiePtr;

	size += 16;								// make room for our cookie & whatever else (also keep to 16-byte alignment!)

	pr = NewPtr(size);						// alloc in Application
	if (pr == nil)
		DoFatalAlert("AllocPtr: NewPtr failed");

	cookiePtr = (u_long *)pr;

	*cookiePtr = 'FACE';

	pr += 16;

	gNumPointers++;

	return(pr);
}


/****************** ALLOC PTR CLEAR ********************/

void *AllocPtrClear(long size)
{
Ptr	pr;
u_long	*cookiePtr;

	size += 16;								// make room for our cookie & whatever else (also keep to 16-byte alignment!)

	pr = NewPtrClear(size);						// alloc in Application
	if (pr == nil)
		DoFatalAlert("AllocPtr: NewPtr failed");

	cookiePtr = (u_long *)pr;

	*cookiePtr = 'FACE';

	pr += 16;

	gNumPointers++;

	return(pr);
}


/***************** SAFE DISPOSE PTR ***********************/

void SafeDisposePtr(Ptr ptr)
{
u_long	*cookiePtr;

	ptr -= 16;					// back up to pt to cookie

	cookiePtr = (u_long *)ptr;

	if (*cookiePtr != 'FACE')
		DoFatalAlert("SafeSafeDisposePtr: invalid cookie!");

	*cookiePtr = 0;

	DisposePtr(ptr);

	gNumPointers--;
}



#pragma mark -

/******************* COPY P STRING ********************/

void CopyPString(Str255 from, Str255 to)
{
short	i,n;

	n = from[0];			// get length

	for (i = 0; i <= n; i++)
		to[i] = from[i];

}


/***************** P STRING TO C ************************/

void PStringToC(char *pString, char *cString)
{
Byte	pLength,i;

	pLength = pString[0];

	for (i=0; i < pLength; i++)					// copy string
		cString[i] = pString[i+1];

	cString[pLength] = 0x00;					// add null character to end of c string
}


/******************* VERIFY SYSTEM ******************/

void VerifySystem(void)
{
OSErr iErr;
long createdDirID;

			/* CHECK PREFERENCES FOLDER */

	iErr = FindFolder(kOnSystemDisk,kPreferencesFolderType,kDontCreateFolder,		// locate the folder
					&gPrefsFolderVRefNum,&gPrefsFolderDirID);
	if (iErr != noErr)
		DoAlert("Warning: Cannot locate the Preferences folder.");

	iErr = DirCreate(gPrefsFolderVRefNum,gPrefsFolderDirID,"CroMag",&createdDirID);		// make folder in there
}


/******************** REGULATE SPEED ***************/

void RegulateSpeed(short fps)
{
short	n;
static oldTick = 0;

	n = 60 / fps;
	while ((TickCount() - oldTick) < n) {}			// wait for n ticks
	oldTick = TickCount();							// remember current time
}


/************* COPY PSTR **********************/

void CopyPStr(ConstStr255Param	inSourceStr, StringPtr	outDestStr)
{
short	dataLen = inSourceStr[0] + 1;

	BlockMoveData(inSourceStr, outDestStr, dataLen);
	outDestStr[0] = dataLen - 1;
}





#pragma mark -



/************** CALC FRAMES PER SECOND *****************/
//
// This version uses UpTime() which is only available on PCI Macs.
//

void CalcFramesPerSecond(void)
{
	IMPLEMENT_ME_SOFT();
	gFramesPerSecond = 60;
	gFramesPerSecondFrac = 1.0f/gFramesPerSecond;

#if 0
AbsoluteTime currTime,deltaTime;
static AbsoluteTime time = {0,0};
Nanoseconds	nano;

	currTime = UpTime();

	deltaTime = SubAbsoluteFromAbsolute(currTime, time);
	nano = AbsoluteToNanoseconds(deltaTime);

	gFramesPerSecond = 1000000.0f / (float)nano.lo;
	gFramesPerSecond *= 1000.0f;

	if (gFramesPerSecond < DEFAULT_FPS)			// (avoid divide by 0's later)
		gFramesPerSecond = DEFAULT_FPS;
	gFramesPerSecondFrac = 1.0f/gFramesPerSecond;		// calc fractional for multiplication


	time = currTime;	// reset for next time interval
#endif
}


/********************* IS POWER OF 2 ****************************/

Boolean IsPowerOf2(int num)
{
int		i;

	i = 2;
	do
	{
		if (i == num)				// see if this power of 2 matches
			return(true);
		i *= 2;						// next power of 2
	}while(i <= num);				// search until power is > number

	return(false);
}


#pragma mark -

/******************* MY FLUSH EVENTS **********************/
//
// This call makes damed sure that there are no events anywhere in any event queue.
//

void MyFlushEvents(void)
{
#if 0 //IJ
EventRecord 	theEvent;

	FlushEvents (everyEvent, REMOVE_ALL_EVENTS);
	FlushEventQueue(GetMainEventQueue());

			/* POLL EVENT QUEUE TO BE SURE THINGS ARE FLUSHED OUT */

	while (GetNextEvent(mDownMask|mUpMask|keyDownMask|keyUpMask|autoKeyMask, &theEvent));


	FlushEvents (everyEvent, REMOVE_ALL_EVENTS);
	FlushEventQueue(GetMainEventQueue());
#endif
}