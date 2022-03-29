//
// window.h
//

#define	ALLOW_FADE		(1)

void InitWindowStuff(void);
void MakeFadeEvent(Boolean fadeIn);
void OGL_FadeOutScene(OGLSetupOutputType* setupInfo, void (*drawRoutine)(OGLSetupOutputType*), void (*updateRoutine)(void));
void Enter2D(Boolean pauseDSp);
void Exit2D(void);
void ChangeWindowScale(void);
void SetFullscreenMode(bool enforceDisplayPref);
