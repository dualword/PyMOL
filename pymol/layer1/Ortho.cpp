
/* 
A* -------------------------------------------------------------------
B* This file contains source code for the PyMOL computer program
C* copyright 1998-2000 by Warren Lyford Delano of DeLano Scientific. 
D* -------------------------------------------------------------------
E* It is unlawful to modify or remove this copyright notice.
F* -------------------------------------------------------------------
G* Please see the accompanying LICENSE file for further information. 
H* -------------------------------------------------------------------
I* Additional authors of this source file include:
-* 
-* 
-*
Z* -------------------------------------------------------------------
*/

#include"os_python.h"

#include"os_predef.h"
#include"os_std.h"
#include"os_gl.h"

#include"main.h"
#include"Version.h"
#include"MemoryDebug.h"
#include"Err.h"
#include"Util.h"
#include"ListMacros.h"
#include"Ortho.h"
#include"P.h"
#include"Scene.h"
#include"Executive.h"
#include"ButMode.h"
#include "Seq.h"
#include"Control.h"
#include"Setting.h"
#include"Wizard.h"
#include"Queue.h"
#include"Pop.h"
#include"Seq.h"
#include"Text.h"
#include"PyMOLOptions.h"
#include"PyMOL.h"
#include"Movie.h"
#include "ShaderMgr.h"
#include "Vector.h"
#include "CGO.h"
#include "MyPNG.h"
#include "MacPyMOL.h"

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#define OrthoSaveLines 0xFF
#define OrthoHistoryLines 0xFF

#define cOrthoCharWidth 8
#define cOrthoLeftMargin 3
#define cOrthoBottomMargin 5

#define CMD_QUEUE_MASK 0x3

struct _COrtho {
  Block *Blocks;
  Block *GrabbedBy, *ClickedIn;
  int X, Y, Height, Width;
  int LastX, LastY, LastModifiers;
  int ActiveButton;
  int DrawText;
  int InputFlag;                /* whether or not we have active input on the line */

  OrthoLineType Line[OrthoSaveLines + 1];
  OrthoLineType History[OrthoHistoryLines + 1];
  int HistoryLine, HistoryView;
  int CurLine, CurChar, PromptChar, CursorChar;
  int AutoOverlayStopLine;
  FILE *Pipe;
  char Prompt[255];
  int ShowLines;
  char Saved[OrthoLineLength];
  int SavedPC, SavedCC;
  float TextColor[3], OverlayColor[3], WizardBackColor[3], WizardTextColor[3];
  int DirtyFlag;
  double BusyLast, BusyLastUpdate;
  int BusyStatus[4];
  char BusyMessage[255];
  char *WizardPromptVLA;
  int SplashFlag;
  int HaveSeqViewer;
  BlockRect LoopRect;
  int LoopFlag;
  int cmdNestLevel;
  CQueue *cmdQueue[CMD_QUEUE_MASK + 1], *cmdActiveQueue;
  int cmdActiveBusy;
  CQueue *feedback;
  int Pushed;
  CDeferred *deferred;
  int RenderMode;
  GLint ViewPort[4];
  int WrapXFlag;
  GLenum ActiveGLBuffer;
  double DrawTime, LastDraw;
  int WrapClickSide;            /* ugly kludge for finding click side in geowall stereo mode */

  /* packing information */
  int WizardHeight;
  int TextBottom;

  int IssueViewportWhenReleased;
  GLuint bg_texture_id;
  short bg_texture_needs_update;
  CGO *bgCGO;
  int bgWidth, bgHeight;
  void *bgData;
  CGO *orthoCGO, *orthoFastCGO;
};

int OrthoBackgroundDataIsSet(PyMOLGlobals *G){
  COrtho *I = G->Ortho;
  return (I->bgData && (I->bgWidth > 0 && I->bgHeight > 0));
  //  return (I->bgCGO != NULL && (I->bgWidth > 0 && I->bgHeight > 0));
}
void *OrthoBackgroundDataGet(PyMOLGlobals *G, int *width, int *height){
  COrtho *I = G->Ortho;
  *width = I->bgWidth;
  *height = I->bgHeight;
  return (I->bgData);
}

void OrthoGetSize(PyMOLGlobals *G, int *width, int *height){
  COrtho *I = G->Ortho;
  *width = I->Width;
  *height = I->Height;
}

void OrthoGetBackgroundSize(PyMOLGlobals * G, int *width, int *height){
  COrtho *I = G->Ortho;
  *width = I->bgWidth;
  *height = I->bgHeight;
}

void OrthoParseCurrentLine(PyMOLGlobals * G);

Block *OrthoFindBlock(PyMOLGlobals * G, int x, int y);
void OrthoKeyControl(PyMOLGlobals * G, unsigned char k);
void OrthoKeyAlt(PyMOLGlobals * G, unsigned char k);
void OrthoKeyCtSh(PyMOLGlobals * G, unsigned char k);
void OrthoKeyCmmd(PyMOLGlobals * G, unsigned char k);



#define cBusyWidth 240
#define cBusyHeight 60
#define cBusyMargin 10
#define cBusyBar 10
#define cBusySpacing 15

#define cBusyUpdate 0.2

#define cWizardTopMargin 15
#define cWizardLeftMargin 15
#define cWizardBorder 7

static int get_wrap_x(int x, int *last_x, int width, int *click_side)
{
  int width_2 = width / 2;
  int width_3 = width / 3;
  if(!last_x) {
    if(x > width_2) {
      x -= width_2;
      if(click_side)
        *click_side = 1;
    } else {
      if(click_side)
        *click_side = -1;
    }
  } else {
    if((x - (*last_x)) > width_3) {
      x -= width_2;
      if(click_side)
        *click_side = 1;
    } else if(((*last_x) - x) > width_3) {
      x += width_2;
      if(click_side)
        *click_side = 1;
    } else {
      if(click_side)
        *click_side = -1;
    }
  }
  return x;
}

void OrthoDrawBuffer(PyMOLGlobals * G, GLenum mode)
{
  COrtho *I = G->Ortho;
  if((mode != I->ActiveGLBuffer) && G->HaveGUI && G->ValidContext) {
#ifndef PURE_OPENGL_ES_2
    if(glGetError()) {
      PRINTFB(G, FB_OpenGL, FB_Warnings)
        " WARNING: BEFORE glDrawBuffer caused GL error\n" ENDFB(G);
    }
    glDrawBuffer(mode);
    if(glGetError()) {
      PRINTFB(G, FB_OpenGL, FB_Warnings)
        " WARNING: glDrawBuffer caused GL error\n" ENDFB(G);
    }
#endif
    I->ActiveGLBuffer = mode;
  }
}

int OrthoGetDirty(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  return I->DirtyFlag;
}

int OrthoGetRenderMode(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  return I->RenderMode;
}

void OrthoSetLoopRect(PyMOLGlobals * G, int flag, BlockRect * rect)
{
  COrtho *I = G->Ortho;
  I->LoopRect = (*rect);
  I->LoopFlag = flag;
  OrthoInvalidateDoDraw(G);
  OrthoDirty(G);
}

int OrthoDeferredWaiting(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  return (I->deferred != NULL);
}

void OrthoExecDeferred(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  CDeferred *deferred = I->deferred;

  I->deferred = NULL;
  /* execute all deferred actions that happened to require a
   * valid OpenGL context (such as atom picks, etc.) */

  DeferredExec(deferred);
}

void OrthoDefer(PyMOLGlobals * G, CDeferred * D)
{
  COrtho *I = G->Ortho;
  CDeferred *d = I->deferred;
  if(d) {
    while(d->next)
      d = d->next;
    d->next = D;
  } else {
    I->deferred = D;
  }
  OrthoDirty(G);
}

int OrthoGetWidth(PyMOLGlobals * G)
{
  if(G) {
    COrtho *I = G->Ortho;
    return (I->Width);
  }
  return 0;
}

int OrthoGetHeight(PyMOLGlobals * G)
{
  if(G) {
    COrtho *I = G->Ortho;
    return (I->Height);
  }
  return 0;
}


/*========================================================================*/
void OrthoFakeDrag(PyMOLGlobals * G)
{                               /* for timing-based events, such as pop-ups */
  COrtho *I = G->Ortho;
  if(I->GrabbedBy)
    OrthoDrag(G, I->LastX, I->LastY, I->LastModifiers);
}


/*========================================================================*/

void OrthoSetWizardPrompt(PyMOLGlobals * G, char *vla)
{
  COrtho *I = G->Ortho;
  VLAFreeP(I->WizardPromptVLA);
  I->WizardPromptVLA = vla;
}


/*========================================================================*/
void OrthoSpecial(PyMOLGlobals * G, int k, int x, int y, int mod)
{
  COrtho *I = G->Ortho;
  int curLine = I->CurLine & OrthoSaveLines;
  int cursorMoved = false;

  PRINTFB(G, FB_Ortho, FB_Blather)
    " OrthoSpecial: %c (%d), x %d y %d, mod %d\n", k, k, x, y, mod ENDFB(G);

  switch (k) {
  case P_GLUT_KEY_DOWN:
    if(I->CurChar && (I->HistoryView == I->HistoryLine)) {
      strcpy(I->History[I->HistoryLine], I->Line[curLine] + I->PromptChar);
    }
    I->HistoryView = (I->HistoryView + 1) & OrthoHistoryLines;
    strcpy(I->Line[curLine], I->Prompt);
    I->PromptChar = strlen(I->Prompt);
    if(I->History[I->HistoryView][0]) {
      strcat(I->Line[curLine], I->History[I->HistoryView]);
      I->CurChar = strlen(I->Line[curLine]);
    } else {
      I->CurChar = I->PromptChar;
    }
    I->InputFlag = 1;
    I->CursorChar = -1;
    cursorMoved = true;
    break;
  case P_GLUT_KEY_UP:
    if(I->CurChar && (I->HistoryView == I->HistoryLine)) {
      strcpy(I->History[I->HistoryLine], I->Line[curLine] + I->PromptChar);
    }
    I->HistoryView = (I->HistoryView - 1) & OrthoHistoryLines;
    strcpy(I->Line[curLine], I->Prompt);
    I->PromptChar = strlen(I->Prompt);
    if(I->History[I->HistoryView][0]) {
      strcat(I->Line[curLine], I->History[I->HistoryView]);
      I->CurChar = strlen(I->Line[curLine]);
    } else {
      I->CurChar = I->PromptChar;
    }
    I->CursorChar = -1;
    I->InputFlag = 1;
    cursorMoved = true;
    break;
  case P_GLUT_KEY_LEFT:
    if(I->CursorChar >= 0) {
      I->CursorChar--;
    } else {
      I->CursorChar = I->CurChar - 1;
    }
    if(I->CursorChar < I->PromptChar)
      I->CursorChar = I->PromptChar;
    cursorMoved = true;
    break;
  case P_GLUT_KEY_RIGHT:
    if(I->CursorChar >= 0) {
      I->CursorChar++;
    } else {
      I->CursorChar = I->CurChar - 1;
    }
    if((unsigned) I->CursorChar > strlen(I->Line[curLine]))
      I->CursorChar = strlen(I->Line[curLine]);
    cursorMoved = true;
    break;
  }
  if (cursorMoved){
    OrthoInvalidateDoDraw(G);    
  }
  OrthoDirty(G);
}


/*========================================================================*/
int OrthoTextVisible(PyMOLGlobals * G)
{
  return (SettingGetGlobal_i(G, cSetting_internal_feedback) ||
          SettingGetGlobal_b(G, cSetting_text) || SettingGetGlobal_i(G, cSetting_overlay));
}


/*========================================================================*/

int OrthoArrowsGrabbed(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  return ((I->CurChar > I->PromptChar) && OrthoTextVisible(G));
  /* arrows can't be grabbed if text isn't visible */
}

/*========================================================================*/
int OrthoGetOverlayStatus(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  int overlay = SettingGetGlobal_i(G, cSetting_overlay);
  if(!overlay) {
    if(SettingGetGlobal_i(G, cSetting_auto_overlay) > 0) {
      if(I->CurLine != I->AutoOverlayStopLine) {
        overlay = -1;           /* signal auto overlay */
      }
    }
  }
  return overlay;
}


/*========================================================================*/
void OrthoRemoveAutoOverlay(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  I->AutoOverlayStopLine = I->CurLine;
}


/*========================================================================*/
void OrthoRemoveSplash(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  I->SplashFlag = false;
}


/*========================================================================*/
void OrthoCommandNest(PyMOLGlobals * G, int dir)
{
  COrtho *I = G->Ortho;
  I->cmdNestLevel += dir;
  {
    int level = I->cmdNestLevel;
    if(level < 0)
      level = 0;
    if(level > CMD_QUEUE_MASK)
      level = CMD_QUEUE_MASK;
    I->cmdActiveQueue = I->cmdQueue[level];
  }
}


/*========================================================================*/
int OrthoCommandOutSize(PyMOLGlobals * G){
  if(G) {
    COrtho *I = G->Ortho;
    if(I && I->cmdActiveQueue) {
      return QueueStrCheck(I->cmdActiveQueue);
    }
  }
  return 0;
}

int OrthoCommandOut(PyMOLGlobals * G, char *buffer)
{
  if(G && buffer) {
    COrtho *I = G->Ortho;

    if(I && I->cmdActiveQueue) {
      int result;
      result = QueueStrOut(I->cmdActiveQueue, buffer);
      return (result);
    }
  }
  return 0;
}


/*========================================================================*/
int OrthoCommandWaiting(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  return (I->cmdActiveBusy || QueueStrCheck(I->cmdActiveQueue));
}


/*========================================================================*/
void OrthoClear(PyMOLGlobals * G)
{
  int a;
  COrtho *I = G->Ortho;
  for(a = 0; a <= OrthoSaveLines; a++)
    I->Line[a][0] = 0;
  OrthoNewLine(G, NULL, true);
  OrthoRestorePrompt(G);
  OrthoInvalidateDoDraw(G);
  OrthoDirty(G);
}


/*========================================================================*/
void OrthoFeedbackIn(PyMOLGlobals * G, const char *buffer)
{
  COrtho *I = G->Ortho;
  if(G->HaveGUI) {
    if(I->feedback)
      QueueStrIn(I->feedback, buffer);
  }
}


/*========================================================================*/
int OrthoFeedbackOut(PyMOLGlobals * G, char *buffer)
{
  COrtho *I = G->Ortho;
  if(I->feedback)
    return (QueueStrOut(I->feedback, buffer));
  else
    return (0);
}


/*========================================================================*/
void OrthoDirty(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  PRINTFD(G, FB_Ortho)
    " OrthoDirty: called.\n" ENDFD;
  if(!I->DirtyFlag) {
    I->DirtyFlag = true;
  }
  PyMOL_NeedRedisplay(G->PyMOL);
}


/*========================================================================*/
void OrthoBusyMessage(PyMOLGlobals * G, const char *message)
{
  COrtho *I = G->Ortho;
  if(strlen(message) < 255)
    strcpy(I->BusyMessage, message);
}


/*========================================================================*/
void OrthoBusySlow(PyMOLGlobals * G, int progress, int total)
{
  COrtho *I = G->Ortho;
  double time_yet = (-I->BusyLastUpdate) + UtilGetSeconds(G);

  PRINTFD(G, FB_Ortho)
    " OrthoBusySlow-DEBUG: progress %d total %d\n", progress, total ENDFD;
  I->BusyStatus[0] = progress;
  I->BusyStatus[1] = total;
  if(SettingGetGlobal_b(G, cSetting_show_progress) && (time_yet > 0.15F)) {
    if(PyMOL_GetBusy(G->PyMOL, false)) {        /* harmless race condition */
#ifndef _PYMOL_NOPY
      int blocked = PAutoBlock(G);
      if(PLockStatusAttempt(G)) {
#endif
        PyMOL_SetProgress(G->PyMOL, PYMOL_PROGRESS_SLOW, progress, total);
        I->BusyLastUpdate = UtilGetSeconds(G);

#ifndef _PYMOL_NOPY
        PUnlockStatus(G);
      }
      PAutoUnblock(G, blocked);
#endif
    }
    OrthoBusyDraw(G, false);
  }
}


/*========================================================================*/
void OrthoBusyFast(PyMOLGlobals * G, int progress, int total)
{
  COrtho *I = G->Ortho;
  double time_yet = (-I->BusyLastUpdate) + UtilGetSeconds(G);
  short finished = progress == total;
  PRINTFD(G, FB_Ortho)
    " OrthoBusyFast-DEBUG: progress %d total %d\n", progress, total ENDFD;
  I->BusyStatus[2] = progress;
  I->BusyStatus[3] = total;
  if(finished || (SettingGetGlobal_b(G, cSetting_show_progress) && (time_yet > 0.15F))) {
    if(PyMOL_GetBusy(G->PyMOL, false) || finished) {        /* harmless race condition */
#ifndef _PYMOL_NOPY
      int blocked = PAutoBlock(G);
      if(PLockStatusAttempt(G)) {
#endif
        PyMOL_SetProgress(G->PyMOL, PYMOL_PROGRESS_FAST, progress, total);
        I->BusyLastUpdate = UtilGetSeconds(G);
#ifndef _PYMOL_NOPY
        PUnlockStatus(G);
      }
      PAutoUnblock(G, blocked);
#endif
    }
    OrthoBusyDraw(G, false);
  }
}


/*========================================================================*/
void OrthoBusyPrime(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  int a;
  for(a = 0; a < 4; a++)
    I->BusyStatus[a] = 0;
  I->BusyMessage[0] = 0;
  I->BusyLast = UtilGetSeconds(G);
  I->BusyLastUpdate = UtilGetSeconds(G);
}


/*========================================================================*/
void OrthoBusyDraw(PyMOLGlobals * G, int force)
{
  COrtho *I = G->Ortho;
  double now;
  double busyTime;

  PRINTFD(G, FB_Ortho)
    " OrthoBusyDraw: entered.\n" ENDFD;
  now = UtilGetSeconds(G);
  busyTime = (-I->BusyLast) + now;
  if(SettingGetGlobal_b(G, cSetting_show_progress) && (force || (busyTime > cBusyUpdate))) {

    I->BusyLast = now;
    if(PIsGlutThread()) {

#ifdef _MACPYMOL_XCODE
      /* BEGIN PROPRIETARY CODE SEGMENT (see disclaimer in "os_proprietary.h") */
      float busyValue = 0.0F;
      if(I->BusyStatus[1]) {
        busyValue = (I->BusyStatus[0] * 1.0F / I->BusyStatus[1]);
      }
      if(I->BusyStatus[3]) {
        busyValue = (I->BusyStatus[2] * 1.0F / I->BusyStatus[3]);
      }
      MacPyMOL_SetProgress(busyValue);
      /* END PROPRIETARY CODE SEGMENT */
#else
      if(G->HaveGUI && G->ValidContext) {
        char *c;
        int x, y;
        float white[3] = { 1, 1, 1 };
        int draw_both = SceneMustDrawBoth(G);
	CGO *orthoCGO = I->orthoCGO;
        OrthoPushMatrix(G);
        {
          int pass = 0;
          SceneGLClear(G, GL_DEPTH_BUFFER_BIT);
          while(1) {
            if(draw_both) {
              if(!pass)
                OrthoDrawBuffer(G, GL_FRONT_LEFT);
              else
                OrthoDrawBuffer(G, GL_FRONT_RIGHT);
            } else {
              OrthoDrawBuffer(G, GL_FRONT);     /* draw into the front buffer */
            }

#ifndef PURE_OPENGL_ES_2
            glColor3f(0.f, 0.f, 0.f); // black
            glBegin(GL_TRIANGLE_STRIP);
            glVertex2i(0, I->Height);
            glVertex2i(cBusyWidth, I->Height);
            glVertex2i(0, I->Height - cBusyHeight);
            glVertex2i(cBusyWidth, I->Height - cBusyHeight);
            glEnd();
            glColor3fv(white);
#endif
            y = I->Height - cBusyMargin;
            c = I->BusyMessage;
            if(*c) {
              TextSetColor(G, white);
              TextSetPos2i(G, cBusyMargin, y - (cBusySpacing / 2));
              TextDrawStr(G, c ORTHOCGOARGVAR);
              y -= cBusySpacing;
            }

            if(I->BusyStatus[1]) {
              glBegin(GL_LINE_LOOP);
              glVertex2i(cBusyMargin, y);
              glVertex2i(cBusyWidth - cBusyMargin, y);
              glVertex2i(cBusyWidth - cBusyMargin, y - cBusyBar);
              glVertex2i(cBusyMargin, y - cBusyBar);
              glEnd();
              glColor3fv(white);
              x =
                (I->BusyStatus[0] * (cBusyWidth - 2 * cBusyMargin) / I->BusyStatus[1]) +
                cBusyMargin;
              glBegin(GL_TRIANGLE_STRIP);
              glVertex2i(cBusyMargin, y);
              glVertex2i(x,y);
              glVertex2i(cBusyMargin, y - cBusyBar);
              glVertex2i(x, y - cBusyBar);
              glEnd();
              y -= cBusySpacing;
            }

            if(I->BusyStatus[3]) {
              glColor3fv(white);
              glBegin(GL_LINE_LOOP);
              glVertex2i(cBusyMargin, y);
              glVertex2i(cBusyWidth - cBusyMargin, y);
              glVertex2i(cBusyWidth - cBusyMargin, y - cBusyBar);
              glVertex2i(cBusyMargin, y - cBusyBar);
              glEnd();
              x =
                (I->BusyStatus[2] * (cBusyWidth - 2 * cBusyMargin) / I->BusyStatus[3]) +
                cBusyMargin;
              glColor3fv(white);
              glBegin(GL_TRIANGLE_STRIP);
              glVertex2i(cBusyMargin, y);
              glVertex2i(x, y);
              glVertex2i(cBusyMargin, y - cBusyBar);
              glVertex2i(x, y - cBusyBar);
              glEnd();
              y -= cBusySpacing;
            }
            if(!draw_both)
              break;
            if(pass > 1)
              break;
            pass++;
          }

          glFlush();
          glFinish();

          if(draw_both)
            OrthoDrawBuffer(G, GL_BACK_LEFT);
          else
            OrthoDrawBuffer(G, GL_BACK);
        }
        OrthoPopMatrix(G);
        OrthoDirty(G);
      }
#endif

    }
  }

  PRINTFD(G, FB_Ortho)
    " OrthoBusyDraw: leaving...\n" ENDFD;

}


/*========================================================================*/
void OrthoRestorePrompt(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  int curLine;
  if(!I->InputFlag) {
    if(I->Saved[0]) {
      if(I->CurChar) {
        OrthoNewLine(G, NULL, true);
      }
      curLine = I->CurLine & OrthoSaveLines;
      strcpy(I->Line[curLine], I->Saved);
      I->Saved[0] = 0;
      I->CurChar = I->SavedCC;
      I->PromptChar = I->SavedPC;
    } else {
      if(I->CurChar)
        OrthoNewLine(G, I->Prompt, true);
      else {
        curLine = I->CurLine & OrthoSaveLines;
        strcpy(I->Line[curLine], I->Prompt);
        I->CurChar = (I->PromptChar = strlen(I->Prompt));
      }
    }
    I->InputFlag = 1;
  }
}


/*========================================================================*/
void OrthoKeyControl(PyMOLGlobals * G, unsigned char k)
{
  char buffer[OrthoLineLength];

  /* safer... */

  sprintf(buffer, "cmd._ctrl(chr(%d))", k);
  /* sprintf(buffer,"_ctrl %c",k); */
  PLog(G, buffer, cPLog_pym);
  PParse(G, buffer);
  PFlush(G);

}

/*========================================================================*/
void OrthoKeyCmmd(PyMOLGlobals * G, unsigned char k)
{
  char buffer[OrthoLineLength];

  /* safer... */

  sprintf(buffer, "cmd._cmmd(chr(%d))", k);
  /* sprintf(buffer,"_ctrl %c",k); */
  PLog(G, buffer, cPLog_pym);
  PParse(G, buffer);
  PFlush(G);

}

/*========================================================================*/
void OrthoKeyCtSh(PyMOLGlobals * G, unsigned char k)
{
  char buffer[OrthoLineLength];

  /* safer... */

  sprintf(buffer, "cmd._ctsh(chr(%d))", k);
  /* sprintf(buffer,"_ctrl %c",k); */
  PLog(G, buffer, cPLog_pym);
  PParse(G, buffer);
  PFlush(G);

}


/*========================================================================*/
void OrthoKeyAlt(PyMOLGlobals * G, unsigned char k)
{
  char buffer[OrthoLineLength];

  /* safer... */

  if(k == '@') {
    /* option G produces '@' on some non-US keyboards, so simply
       ignore the modifier */
    OrthoKey(G, k, 0, 0, 0);
  } else {
    sprintf(buffer, "cmd._alt(chr(%d))", k);
    /* sprintf(buffer,"_alt %c",k); */
    PLog(G, buffer, cPLog_pym);
    PParse(G, buffer);
    PFlush(G);
  }

}

static int add_normal_char(COrtho * I, unsigned char k)
{
  char buffer[OrthoLineLength];
  int curLine = I->CurLine & OrthoSaveLines;
  if(I->CursorChar >= 0) {
    strcpy(buffer, I->Line[curLine] + I->CursorChar);
    I->Line[curLine][I->CursorChar] = k;
    I->CursorChar++;
    I->CurChar++;
    strcpy(I->Line[curLine] + I->CursorChar, buffer);
  } else {
    I->Line[curLine][I->CurChar] = k;
    I->CurChar++;
    I->Line[curLine][I->CurChar] = 0;
  }
  return curLine;
}


/*========================================================================*/
void OrthoKey(PyMOLGlobals * G, unsigned char k, int x, int y, int mod)
{
  COrtho *I = G->Ortho;
  char buffer[OrthoLineLength];
  int curLine;

  PRINTFB(G, FB_Ortho, FB_Blather)
    " OrthoKey: %c (%d), x %d y %d, mod %d\n", k, k, x, y, mod ENDFB(G);

  if(!I->InputFlag) {
    if(I->Saved[0]) {
      if(I->CurChar) {
        OrthoNewLine(G, NULL, true);
      }
      curLine = I->CurLine & OrthoSaveLines;
      strcpy(I->Line[curLine], I->Saved);
      I->Saved[0] = 0;
      I->CurChar = I->SavedCC;
      I->PromptChar = I->SavedPC;
    } else {
      if(I->CurChar) {
        OrthoNewLine(G, I->Prompt, true);
      } else {
        curLine = I->CurLine & OrthoSaveLines;
        strcpy(I->Line[curLine], I->Prompt);
        I->CurChar = (I->PromptChar = strlen(I->Prompt));
      }
    }
    I->InputFlag = 1;
  }
  if(mod == 4) {                /* alt */
    OrthoKeyAlt(G, k);
  } else if  (mod == 3) {       /* chsh */
    OrthoKeyCtSh(G,(unsigned int) (k+64));
  } else if((k > 32) && (k != 127)) {
    curLine = add_normal_char(I, k);
  } else
    switch (k) {
    case 32:                   /* spacebar */
      if((!OrthoArrowsGrabbed(G)) &&
	 (I->CurChar == I->PromptChar)) { /* no text entered yet... */
        if(SettingGetGlobal_b(G, cSetting_presentation)) {
          if(mod & cOrthoSHIFT) {
            OrthoCommandIn(G,"rewind;mplay");
          } else {
            PParse(G, "cmd.scene('','next')");
          }
        } else {
          if(mod & cOrthoSHIFT) {
            OrthoCommandIn(G,"rewind;mplay");
          } else {
            OrthoCommandIn(G,"mtoggle");
          }
        }
      } else {
        curLine = add_normal_char(I, k);
      }
      break;
    case 127:                  /* delete */
#if !defined(_PYMOL_OSX) || defined(_PYMOL_LIB)
      /* this defined(_PYMOL_LIB) should really be for JyMOL, not all _PYMOL_LIB, AX? */
      if((!I->CurChar) || (I->CurChar == I->PromptChar) || !OrthoTextVisible(G)) {
        OrthoKeyControl(G, 4 + 64);
      } else {
        if(I->CursorChar >= 0) {
          if(I->CursorChar < I->CurChar)
            I->CursorChar++;
          if(I->CursorChar == I->CurChar)
            I->CursorChar = -1;
        }
        if(I->CurChar > I->PromptChar) {
          curLine = I->CurLine & OrthoSaveLines;
          if(I->CursorChar >= 0) {
            if(I->CursorChar > I->PromptChar) {
              strcpy(buffer, I->Line[curLine] + I->CursorChar);
              I->CursorChar--;
              I->CurChar--;
              strcpy(I->Line[curLine] + I->CursorChar, buffer);
            }
          } else {
            I->CurChar--;
            I->Line[curLine][I->CurChar] = 0;
          }
        }
      }
      break;
    case 8:                    /* backspace */
#endif
      if(I->CurChar > I->PromptChar) {
        curLine = I->CurLine & OrthoSaveLines;
        if(I->CursorChar >= 0) {
          if(I->CursorChar > I->PromptChar) {
            strcpy(buffer, I->Line[curLine] + I->CursorChar);
            I->Line[curLine][I->CursorChar] = k;
            I->CursorChar--;
            I->CurChar--;
            strcpy(I->Line[curLine] + I->CursorChar, buffer);
          }
        } else {
          I->CurChar--;
          I->Line[curLine][I->CurChar] = 0;
        }
      }
      break;
    case 5:                    /* CTRL E -- ending */
      if(OrthoArrowsGrabbed(G)) {
        I->CursorChar = -1;
      } else
        OrthoKeyControl(G, (unsigned char) (k + 64));
      break;
    case 1:                    /* CTRL A -- beginning */
      if(OrthoArrowsGrabbed(G)) {
        if(I->CurChar)
          I->CursorChar = I->PromptChar;
      } else
        OrthoKeyControl(G, (unsigned char) (k + 64));
      break;
    case 4:                    /* CTRL D */
      if((!I->CurChar) || (I->CurChar == I->PromptChar) || !OrthoTextVisible(G)) {
        OrthoKeyControl(G, (unsigned char) (4 + 64));
      } else if((I->CurChar > I->PromptChar) && (I->CursorChar >= 0) && (I->CursorChar < I->CurChar)) { /* deleting */
        curLine = I->CurLine & OrthoSaveLines;
        strcpy(buffer, I->Line[curLine] + I->CursorChar + 1);
        I->CurChar--;
        strcpy(I->Line[curLine] + I->CursorChar, buffer);
      } else {                  /* filename completion query */
        curLine = I->CurLine & OrthoSaveLines;
        if(I->PromptChar) {
          strcpy(buffer, I->Line[curLine]);
          PComplete(G, buffer + I->PromptChar, sizeof(OrthoLineType) - I->PromptChar);      /* just print, don't complete */
        }
      }
      break;
    case 9:                    /* CTRL I -- tab */
      if(mod & cOrthoCTRL) {
        OrthoKeyControl(G, (unsigned char) (k + 64));
      } else {   
        curLine = I->CurLine & OrthoSaveLines;
        if(I->PromptChar) {
          strcpy(buffer, I->Line[curLine]);

          if(PComplete(G, buffer + I->PromptChar, sizeof(OrthoLineType) - I->PromptChar)) {
            OrthoRestorePrompt(G);
            curLine = I->CurLine & OrthoSaveLines;
            strcpy(I->Line[curLine], buffer);
            I->CurChar = strlen(I->Line[curLine]);
          }
        }
      }
      break;
    case 27:                   /* ESCAPE */
      if(SettingGetGlobal_b(G, cSetting_presentation)
         && !(mod & (cOrthoCTRL | cOrthoSHIFT))) {
        PParse(G, "_quit");
      } else {
        if(I->SplashFlag) {
          OrthoRemoveSplash(G);
        } else {
          if(mod & cOrthoSHIFT)
            SettingSetGlobal_i(G, cSetting_overlay, !(SettingGetGlobal_i(G, cSetting_overlay)));
          else
            SettingSetGlobal_b(G, cSetting_text,    !(SettingGetGlobal_b(G, cSetting_text   )));
        }
      }
      break;
    case 13:                   /* CTRL M -- carriage return */
      if(I->CurChar > I->PromptChar)
        OrthoParseCurrentLine(G);
      else if(((SettingGetGlobal_b(G, cSetting_movie_panel) ||
                SettingGetGlobal_b(G, cSetting_presentation)) 
               && MovieGetLength(G))) {
        if(mod & cOrthoSHIFT) {
          if(mod & cOrthoCTRL) 
            OrthoCommandIn(G,"mview toggle_interp,quiet=1,object=same");	    
          else
            OrthoCommandIn(G,"mview toggle_interp,quiet=1");	    
        } else if(mod & cOrthoCTRL) {
          OrthoCommandIn(G,"mview toggle,freeze=1,quiet=1");
        } else {
          if(SettingGetGlobal_b(G, cSetting_presentation)) {
            OrthoCommandIn(G,"mtoggle");  
          } else {
            OrthoCommandIn(G,"mview toggle,quiet=1");
          }
        }
      }
      break;
    case 11:                   /* CTRL K -- truncate */
      if(OrthoArrowsGrabbed(G)) {
        if(I->CursorChar >= 0) {
          I->Line[I->CurLine & OrthoSaveLines][I->CursorChar] = 0;
          I->CurChar = I->CursorChar;
          I->CursorChar = -1;
        }
      } else {
        if(mod & cOrthoCTRL) {
          OrthoKeyControl(G, (unsigned char) (k + 64));
        }
      }
      break;
    case 22:                   /* CTRL V -- paste */
#ifndef _PYMOL_NOPY
      if (I->CurChar != I->PromptChar) { /* no text entered yet... */
	PBlockAndUnlockAPI(G);
	PRunStringInstance(G, "cmd.paste()");
	PLockAPIAndUnblock(G);
      } else {
	OrthoKeyControl(G, (unsigned char) (k + 64));
      }
#endif
      break;
    default:
      OrthoKeyControl(G, (unsigned char) (k + 64));
      break;
    }
  OrthoInvalidateDoDraw(G);
}


/*========================================================================*/
void OrthoParseCurrentLine(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  char buffer[OrthoLineLength];
  int curLine;

  OrthoRemoveAutoOverlay(G);
  curLine = I->CurLine & OrthoSaveLines;
  I->Line[curLine][I->CurChar] = 0;
  strcpy(buffer, I->Line[curLine] + I->PromptChar);
#ifndef _PYMOL_NOPY
  if(buffer[0]) {
    strcpy(I->History[I->HistoryLine], buffer);
    I->HistoryLine = (I->HistoryLine + 1) & OrthoHistoryLines;
    I->History[I->HistoryLine][0] = 0;
    I->HistoryView = I->HistoryLine;
    OrthoNewLine(G, NULL, true);
    if(WordMatch(G, buffer, "quit", true) == 0) /* don't log quit */
      PLog(G, buffer, cPLog_pml);
    OrthoDirty(G);              /* this will force a redraw, if necessary */
    PParse(G, buffer);
    OrthoRestorePrompt(G);
  }
#endif
  I->CursorChar = -1;
}


/*========================================================================*/
void OrthoAddOutput(PyMOLGlobals * G, const char *str)
{
  COrtho *I = G->Ortho;
  int curLine;
  const char *p;
  char *q;
  int cc;
  int wrap;
  curLine = I->CurLine & OrthoSaveLines;
  if(I->InputFlag) {
    strcpy(I->Saved, I->Line[curLine]);
    I->SavedPC = I->PromptChar;
    I->SavedCC = I->CurChar;
    I->PromptChar = 0;
    I->CurChar = 0;
    I->Line[curLine][0] = 0;
    I->InputFlag = 0;
  }
  curLine = I->CurLine & OrthoSaveLines;
  p = str;
  q = I->Line[curLine] + I->CurChar;
  cc = I->CurChar;
  while(*p) {
    if(*p >= 32) {
      cc++;
      wrap = SettingGetGlobal_b(G, cSetting_wrap_output);

      if(wrap > 0) {
        if(cc > wrap) {
          *q = 0;
          I->CurChar = cc;
          OrthoNewLine(G, NULL, true);
          cc = 0;
          q = I->Line[I->CurLine & OrthoSaveLines];
          curLine = I->CurLine & OrthoSaveLines;
        }
      }
      if(cc >= OrthoLineLength - 6) {   /* fail safe */
        *q = 0;
        I->CurChar = cc;
        OrthoNewLine(G, NULL, false);
        cc = 0;
        q = I->Line[I->CurLine & OrthoSaveLines];
        curLine = I->CurLine & OrthoSaveLines;
      }
      *q++ = *p++;
    } else if((*p == 13) || (*p == 10)) {
      *q = 0;
      I->CurChar = cc;
      OrthoNewLine(G, NULL, true);
      q = I->Line[I->CurLine & OrthoSaveLines];
      curLine = I->CurLine & OrthoSaveLines;
      p++;
      cc = 0;
    } else
      p++;
  }
  *q = 0;
  I->CurChar = strlen(I->Line[curLine]);
  if((SettingGetGlobal_i(G, cSetting_internal_feedback) > 1) ||
     SettingGetGlobal_i(G, cSetting_overlay) || SettingGetGlobal_i(G, cSetting_auto_overlay))
    OrthoDirty(G);

  if(I->DrawText)
    OrthoInvalidateDoDraw(G);
}


/*========================================================================*/
void OrthoNewLine(PyMOLGlobals * G, const char *prompt, int crlf)
{
  int curLine;
  COrtho *I = G->Ortho;

  /*  printf("orthoNewLine: CC: %d CL:%d PC: %d IF:L %d\n",I->CurChar,I->CurLine,
     I->PromptChar,I->InputFlag); */
  /*  if(I->CurChar)
     { */
  if(I->CurChar)
    OrthoFeedbackIn(G, I->Line[I->CurLine & OrthoSaveLines]);
  else
    OrthoFeedbackIn(G, " ");
  if(Feedback(G, FB_Python, FB_Output)) {
    if(crlf) {
      printf("%s\n", I->Line[I->CurLine & OrthoSaveLines]);
    } else {
      printf("%s", I->Line[I->CurLine & OrthoSaveLines]);
    }
    fflush(stdout);
  }
  /*        } */

  /*  if(I->Line[I->CurLine&OrthoSaveLines][0]) */
  I->CurLine++;
  curLine = I->CurLine & OrthoSaveLines;

  if(prompt) {
    strcpy(I->Line[curLine], prompt);
    I->CurChar = (I->PromptChar = strlen(prompt));
    I->InputFlag = 1;
  } else {
    I->CurChar = 0;
    I->Line[curLine][0] = 0;
    I->PromptChar = 0;
    I->InputFlag = 0;
  }
  /*printf("orthoNewLine: CC: %d CL:%d PC: %d IF:L %d\n",I->CurChar,I->CurLine,
     I->PromptChar,I->InputFlag); */

}


/*========================================================================*/
void OrthoGrab(PyMOLGlobals * G, Block * block)
{
  COrtho *I = G->Ortho;
  I->GrabbedBy = block;
}

int OrthoGrabbedBy(PyMOLGlobals * G, Block * block)
{
  COrtho *I = G->Ortho;
  return I->GrabbedBy == block;
}

void OrthoDoViewportWhenReleased(PyMOLGlobals *G)
{
  COrtho *I = G->Ortho;
  if(!(I->GrabbedBy||I->ClickedIn)) { /* no active UI element? */
    OrthoCommandIn(G, "viewport"); /* then issue viewport refresh */
    OrthoDirty(G);
  } else {
    I->IssueViewportWhenReleased = true; /* otherwise, defer */
  }
}

/*========================================================================*/
void OrthoUngrab(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  I->GrabbedBy = NULL;
}


/*========================================================================*/
Block *OrthoNewBlock(PyMOLGlobals * G, Block * block)
{
  if(!block)
    ListElemAlloc(G, block, Block);
  UtilZeroMem(block, sizeof(Block));
  BlockInit(G, block);
  return (block);
}


/*========================================================================*/
void OrthoFreeBlock(PyMOLGlobals * G, Block * block)
{
  if(block)
    ListElemFree(block);
}


/*========================================================================*/
void OrthoAttach(PyMOLGlobals * G, Block * block, int type)
{
  COrtho *I = G->Ortho;
  ListInsert(I->Blocks, block, NULL, next, Block);
}


/*========================================================================*/
void OrthoDetach(PyMOLGlobals * G, Block * block)
{
  COrtho *I = G->Ortho;
  if(I->GrabbedBy == block)
    I->GrabbedBy = NULL;
  ListDetach(I->Blocks, block, next, Block);
}

float *OrthoGetOverlayColor(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  return I->OverlayColor;
}


/*========================================================================*/

/* BEGIN PROPRIETARY CODE SEGMENT (see disclaimer in "os_proprietary.h") */
#ifdef PYMOL_EVAL
#include "OrthoEvalMessage.h"
#endif
#ifdef PYMOL_BETA
#include "OrthoBetaMessage.h"
#endif
#ifdef JYMOL_EVAL
#include "OrthoJyMolEvalMessage.h"
#endif
#ifdef PYMOL_EDU
#include "OrthoEduMessage.h"
#endif
#ifdef PYMOL_COLL
#include "OrthoCollMessage.h"
#endif
#ifdef AXPYMOL_EVAL
#include "OrthoAxMessage.h"
#endif

/* END PROPRIETARY CODE SEGMENT */

/* draw background gradient from bg_rgb_top
 * to bg_rgb_bottom is bg_gradient is set
 */

#define BACKGROUND_TEXTURE_SIZE 256

GLuint OrthoGetBackgroundTextureID(PyMOLGlobals * G){
  COrtho *I = G->Ortho;
  return I->bg_texture_id;
}

void OrthoBackgroundTextureNeedsUpdate(PyMOLGlobals * G){
  COrtho *I = G->Ortho;
  I->bg_texture_needs_update = 1;
}

void bg_grad(PyMOLGlobals * G) {
  COrtho *I = G->Ortho;    
  float top[3];
  float bottom[3];
  int bg_gradient = SettingGet_b(G, NULL, NULL, cSetting_bg_gradient);
  short bg_is_solid = 0;
  int ok = true;
  copy3f(ColorGet(G, SettingGet_color(G, NULL, NULL, cSetting_bg_rgb_top)), top);
  copy3f(ColorGet(G, SettingGet_color(G, NULL, NULL, cSetting_bg_rgb_bottom)), bottom);

  if (!bg_gradient){
    float zero[3] = { 0.f, 0.f, 0.f } ;
    float *bg_rgb = ColorGet(G, SettingGet_color(G, NULL, NULL, cSetting_bg_rgb));
    bg_is_solid = !equal3f(bg_rgb, zero);
    if (!bg_is_solid)
      return;
  }

  if (!CShaderMgr_ShadersPresent(G->ShaderMgr)){
    float zero[3] = { 0.f, 0.f, 0.f } ;
    float *bg_rgb = ColorGet(G, SettingGet_color(G, NULL, NULL, cSetting_bg_rgb));
    bg_is_solid = !equal3f(bg_rgb, zero);
    if (bg_is_solid){
      SceneGLClearColor(bg_rgb[0], bg_rgb[1], bg_rgb[2], 1.0);
      glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }
    return;
  }

  glDisable(GL_DEPTH_TEST);

  {
    if (!I->bgCGO) {
      CGO *cgo = CGONew(G), *cgo2 = NULL;
      ok &= CGOBegin(cgo, GL_TRIANGLE_STRIP);
      if (ok)
	ok &= CGOVertex(cgo, -1.f, -1.f, 0.98f);
      if (ok)
	ok &= CGOVertex(cgo, 1.f, -1.f, 0.98f);
      if (ok)
	ok &= CGOVertex(cgo, -1.f, 1.f, 0.98f);
      if (ok)
	ok &= CGOVertex(cgo, 1.f, 1.f, 0.98f);
      if (ok)
	ok &= CGOEnd(cgo);
      if (ok)
	ok &= CGOStop(cgo);
      if (ok)
	cgo2 = CGOCombineBeginEnd(cgo, 0);
      CHECKOK(ok, cgo2);
      CGOFree(cgo);
      if (ok)
	I->bgCGO = CGOOptimizeToVBONotIndexed(cgo2, 0);
      if (ok){
	CGOChangeShadersTo(I->bgCGO, GL_DEFAULT_SHADER, GL_BACKGROUND_SHADER);
	I->bgCGO->use_shader = true;
      } else {
	CGOFree(I->bgCGO);
	I->bgCGO = NULL;
      }
      CGOFree(cgo2);
    }
    if (ok && !bg_is_solid && (I->bgData && (!I->bg_texture_id || I->bg_texture_needs_update))){
      short is_new = !I->bg_texture_id;
      if (is_new){
	glGenTextures(1, &I->bg_texture_id);
      }
      glActiveTexture(GL_TEXTURE4);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glBindTexture(GL_TEXTURE_2D, I->bg_texture_id);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      {
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      }
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		   I->bgWidth, I->bgHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)I->bgData);

      bg_gradient = I->bg_texture_needs_update = 0;
    }

    if (ok && !bg_is_solid && bg_gradient && (!I->bg_texture_id || I->bg_texture_needs_update)){
      short is_new = !I->bg_texture_id;
      int tex_dim = BACKGROUND_TEXTURE_SIZE;
      int buff_total = tex_dim * tex_dim;
      unsigned char *temp_buffer = Alloc(unsigned char, buff_total * 4);
      I->bg_texture_needs_update = 0;
      I->bgWidth = BACKGROUND_TEXTURE_SIZE;
      I->bgHeight = BACKGROUND_TEXTURE_SIZE;
      if (is_new){
	glGenTextures(1, &I->bg_texture_id);
      }
      glActiveTexture(GL_TEXTURE4);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glBindTexture(GL_TEXTURE_2D, I->bg_texture_id);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      {
	int bg_image_linear = SettingGet_b(G, NULL, NULL, cSetting_bg_image_linear);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bg_image_linear ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, bg_image_linear ? GL_LINEAR : GL_NEAREST);
      }
      UtilZeroMem(temp_buffer, buff_total * 4);

      {
	int a, b;
	unsigned char *q, val[4];
	float bot[3] = { bottom[0]*255, bottom[1]*255, bottom[2]*255 };
	float tmpb, diff[3] = { 255.f*(top[0] - bottom[0]), 
				255.f*(top[1] - bottom[1]), 
				255.f*(top[2] - bottom[2]) };
          
	for(b = 0; b < BACKGROUND_TEXTURE_SIZE; b++) {
	  tmpb = b / (BACKGROUND_TEXTURE_SIZE-1.f);
	  val[0] = (unsigned char)pymol_roundf(bot[0] + tmpb*diff[0]) ;
	  val[1] = (unsigned char)pymol_roundf(bot[1] + tmpb*diff[1]) ;
	  val[2] = (unsigned char)pymol_roundf(bot[2] + tmpb*diff[2]) ;
	  for(a = 0; a < BACKGROUND_TEXTURE_SIZE; a++) {
	    q = temp_buffer + (4 * BACKGROUND_TEXTURE_SIZE * b) + 4 * a;
	    *(q++) = val[0];
	    *(q++) = val[1];
	    *(q++) = val[2];
	    *(q++) = 255;
	  }
	}
      }
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		   tex_dim, tex_dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)temp_buffer);
      FreeP(temp_buffer);
    }
    if (ok && I->bgCGO) {
      CShaderPrg *shaderPrg = CShaderPrg_Get_BackgroundShader(G);
      if (shaderPrg){
       	CGORenderGL(I->bgCGO, NULL, NULL, NULL, NULL, NULL);
	CShaderPrg_Disable(shaderPrg);
	glEnable(GL_DEPTH_TEST);
      }
    }
  }
  glEnable(GL_DEPTH_TEST);
}

void OrthoDoDraw(PyMOLGlobals * G, int render_mode)
{
  COrtho *I = G->Ortho;
  CGO *orthoCGO = NULL;
  int x, y;
  int l, lcount;
  char *str;
  float *v;
  int showLines;
  int height;
  int overlay, text;
  int rightSceneMargin;
  int internal_feedback;
  int times = 1, origtimes = 0;
  int double_pump = false;
  float *bg_color;
  int skip_prompt = 0;
  int render = false;
  int internal_gui_mode = SettingGetGlobal_i(G, cSetting_internal_gui_mode);

  int generate_shader_cgo = 0;
  I->RenderMode = render_mode;
  if(SettingGetGlobal_b(G, cSetting_seq_view)) {
    SeqUpdate(G);
    I->HaveSeqViewer = true;
  } else if(I->HaveSeqViewer) {
    SeqUpdate(G);
    I->HaveSeqViewer = false;
  }

  if(SettingGet_i(G, NULL, NULL, cSetting_internal_prompt))
    skip_prompt = 0;
  else
    skip_prompt = 1;

  double_pump = SettingGet_i(G, NULL, NULL, cSetting_stereo_double_pump_mono);
  bg_color = ColorGet(G, SettingGet_color(G, NULL, NULL, cSetting_bg_rgb));

  I->OverlayColor[0] = 1.0F - bg_color[0];
  I->OverlayColor[1] = 1.0F - bg_color[1];
  I->OverlayColor[2] = 1.0F - bg_color[2];
  if(diff3f(I->OverlayColor, bg_color) < 0.25)
    zero3f(I->OverlayColor);

  PRINTFD(G, FB_Ortho)
    " OrthoDoDraw: entered.\n" ENDFD;
  if(G->HaveGUI && G->ValidContext) {

    if(Feedback(G, FB_OpenGL, FB_Debugging))
      PyMOLCheckOpenGLErr("OrthoDoDraw checkpoint 0");

    if(SettingGetGlobal_b(G, cSetting_internal_gui)) {
      switch (SettingGetGlobal_i(G, cSetting_internal_gui_mode)) {
      case 0:
        rightSceneMargin = SettingGetGlobal_i(G, cSetting_internal_gui_width);
        break;
      default:
        rightSceneMargin = 0;
        break;
      }
    } else {
      rightSceneMargin = 0;
    }

    internal_feedback = SettingGetGlobal_i(G, cSetting_internal_feedback);

    v = ColorGet(G, SettingGet_color(G, NULL, NULL, cSetting_bg_rgb));
    overlay = OrthoGetOverlayStatus(G);
    switch (overlay) {
    case -1:                   /* auto overlay */
      overlay = I->CurLine - I->AutoOverlayStopLine;
      if(overlay < 0) {
        overlay += (OrthoSaveLines + 1);
      }
      if(internal_feedback > 1) {
        overlay -= (internal_feedback - 1);
      }
      if(overlay < 0)
        overlay = 0;
      break;
    case 1:                    /* default -- user overlay_lines */
      overlay = SettingGetGlobal_i(G, cSetting_overlay_lines);
      break;
    }

    text = SettingGetGlobal_b(G, cSetting_text);
    if(text)
      overlay = 0;

    if(overlay || (!text))
      if(!SceneRenderCached(G))
        render = true;

    if(render_mode < 2) {
      if(SceneMustDrawBoth(G)) {
        OrthoDrawBuffer(G, GL_BACK_LEFT);
        SceneGLClear(G, GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        OrthoDrawBuffer(G, GL_BACK_RIGHT);
        SceneGLClear(G, GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        times = 2;
        double_pump = true;
      } else {
        OrthoDrawBuffer(G, GL_BACK);
        SceneGLClear(G, GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        times = 1;
        double_pump = false;
      }
    } else {
      times = 1;
      double_pump = false;
    }

    I->DrawTime = -I->LastDraw;
    I->LastDraw = UtilGetSeconds(G);
    I->DrawTime += I->LastDraw;
    ButModeSetRate(G, (float) I->DrawTime);

    if(render && (render_mode < 2))
      SceneRender(G, NULL, 0, 0, NULL, 0, 0, 0,
                  SettingGetGlobal_b(G, cSetting_image_copy_always), 0);
    else if (text){
      SceneRender(G, NULL, 0, 0, NULL, 0, 0, 0,
                  SettingGetGlobal_b(G, cSetting_image_copy_always), 1 /* just_background */);
    }
    SceneGLClearColor(0.0, 0.0, 0.0, 1.0);

    origtimes = times;
    while(times--) {

      switch (times) {
      case 1:
        OrthoDrawBuffer(G, GL_BACK_LEFT);

        break;
      case 0:
        if(double_pump) {
          OrthoDrawBuffer(G, GL_BACK_RIGHT);
        } else
          OrthoDrawBuffer(G, GL_BACK);
        break;
      }

      OrthoPushMatrix(G);

      if (CShaderMgr_ShadersPresent(G->ShaderMgr)){
	if(SettingGetGlobal_b(G, cSetting_internal_gui) && 
	   SettingGetGlobal_b(G, cSetting_use_shaders)){
	  CGO *orthoFastCGO = CGONew(G);
	  if (I->orthoFastCGO)
	    CGOFree(I->orthoFastCGO);
	  I->orthoFastCGO = NULL;
	  if (BlockRecursiveFastDraw(I->Blocks ORTHOFASTCGOARGVAR)){
	    int ok = true;
	    CGO *expandedCGO;
	    CGOStop(orthoFastCGO);
	    expandedCGO = CGOExpandDrawTextures(orthoFastCGO, 0);
	    CHECKOK(ok, expandedCGO);
	    if (ok)
	      I->orthoFastCGO = CGOOptimizeScreenTexturesAndPolygons(expandedCGO, 0);
	    CHECKOK(ok, I->orthoFastCGO);
	    CGOFree(orthoFastCGO);
	    CGOFree(expandedCGO);
	    if (ok){
	      CGOStop(I->orthoFastCGO);
	      I->orthoFastCGO->use_shader = true;
	    } else {
	      CGOFree(I->orthoFastCGO);
	    }
	  } else {
	    CGOFree(orthoFastCGO);
	    orthoFastCGO = NULL;
	  }
	  if (!I->orthoCGO){
	    orthoCGO = CGONew(G);
	    generate_shader_cgo = true;
	  } else {
	    OrthoRenderCGO(G);
	    OrthoPopMatrix(G);
	    continue;
	  }
	}
      }
      x = I->X;
      y = I->Y;

      if(I->DrawText && internal_feedback) {    /* moved to avoid conflict with menus */
        Block *block = SceneGetBlock(G);
        height = block->rect.bottom;
        switch (internal_gui_mode) {
        case 0:
	  if (generate_shader_cgo){
	    CGOColor(orthoCGO, 0.f, 0.f, 0.f);
	    CGOBegin(orthoCGO, GL_TRIANGLE_STRIP);
	    CGOVertex(orthoCGO, I->Width - rightSceneMargin, height - 1, 0.f);
	    CGOVertex(orthoCGO, I->Width - rightSceneMargin, 0, 0.f);
	    CGOVertex(orthoCGO, 0.f, height - 1,0.f);
	    CGOVertex(orthoCGO, 0.f, 0.f, 0.f);
	    CGOEnd(orthoCGO);
	  } else {
	    glColor3f(0.0, 0.0, 0.0);
	    glBegin(GL_POLYGON);
	    glVertex2i(I->Width - rightSceneMargin, height - 1);
	    glVertex2i(I->Width - rightSceneMargin, 0);
	    glVertex2i(0, 0);
	    glVertex2i(0, height - 1);
	    glEnd();
	  }
          /* deliberate fall-through */
        case 1:
	  if (generate_shader_cgo){
	    CGOColor(orthoCGO, 0.3f, 0.3f, 0.3f);
	    CGOBegin(orthoCGO, GL_TRIANGLE_STRIP);
	    CGOVertex(orthoCGO, 1 + I->Width - rightSceneMargin, height, 0.f);
	    CGOVertex(orthoCGO, 1 + I->Width - rightSceneMargin, height - 1, 0.f);
	    CGOVertex(orthoCGO, -1, height, 0.f);
	    CGOVertex(orthoCGO, -1, height - 1, 0.f);
	    CGOEnd(orthoCGO);
	  } else {
	    glColor3f(0.3, 0.3, 0.3);
	    glBegin(GL_LINES);
	    glVertex2i(1 + I->Width - rightSceneMargin, height - 1);
	    glVertex2i(-1, height - 1);
	    glEnd();
	  }
          break;
        }
      }

      PRINTFD(G, FB_Ortho)
        " OrthoDoDraw: drawing blocks...\n" ENDFD;

      if(SettingGetGlobal_b(G, cSetting_internal_gui)) {
        int internal_gui_width = SettingGetGlobal_i(G, cSetting_internal_gui_width);
        if(internal_gui_mode != 2) {
	  if (generate_shader_cgo){
	    CGOColor(orthoCGO, 0.3f, 0.3f, 0.3f);
	    CGOBegin(orthoCGO, GL_TRIANGLE_STRIP);
	    CGOVertex(orthoCGO, I->Width - internal_gui_width, 0.f, 0.f);
	    CGOVertex(orthoCGO, I->Width - internal_gui_width + 1.f, 0.f, 0.f);
	    CGOVertex(orthoCGO, I->Width - internal_gui_width, I->Height, 0.f);
	    CGOVertex(orthoCGO, I->Width - internal_gui_width + 1.f, I->Height, 0.f);
	    CGOEnd(orthoCGO);
	  } else {
	    glColor3f(0.3, 0.3, 0.3);
	    glBegin(GL_LINES);
	    glVertex2i(I->Width - internal_gui_width, 0);
	    glVertex2i(I->Width - internal_gui_width, I->Height);
	    glEnd();
	  }
        }
      }

      OrthoRestorePrompt(G);

      if(I->DrawText) {
        int adjust_at = 0;
        /* now print the text */

        lcount = 0;
        x = cOrthoLeftMargin;
        y = cOrthoBottomMargin + MovieGetPanelHeight(G);

#ifdef _PYMOL_SHARP3D
        if(SceneGetStereo(G) && SettingGetGlobal_b(G, cSetting_overlay)) {
          y += (7 * cOrthoLineHeight) / 10;
        }
#endif
        if(SettingGetGlobal_b(G, cSetting_text) || I->SplashFlag)
          showLines = I->ShowLines;
        else {
          showLines = internal_feedback + overlay;
        }
        if(internal_feedback)
          adjust_at = internal_feedback + 1;

        l = (I->CurLine - (lcount + skip_prompt)) & OrthoSaveLines;

	if (orthoCGO)
	  CGOColorv(orthoCGO, I->TextColor);
	else
	  glColor3fv(I->TextColor);

        while(l >= 0) {
          lcount++;
          if(lcount > showLines)
            break;
          if(lcount == adjust_at)
            y += 4;
          str = I->Line[l & OrthoSaveLines];
          if(internal_gui_mode) {
            TextSetColor(G, I->OverlayColor);
          } else if(strncmp(str, I->Prompt, 6) == 0) {
            if(lcount < adjust_at)
              TextSetColor(G, I->TextColor);
            else {
              if(length3f(I->OverlayColor) < 0.5)
                TextSetColor(G, I->OverlayColor);
              else
                TextSetColor(G, I->TextColor);
            }
          } else
            TextSetColor(G, I->OverlayColor);
          TextSetPos2i(G, x, y);
          if(str) {
            TextDrawStr(G, str ORTHOCGOARGVAR);
            if((lcount == 1) && (I->InputFlag)) {
              if(!skip_prompt) {
                if(I->CursorChar >= 0) {
                  TextSetPos2i(G, x + 8 * I->CursorChar, y);
                }
                TextDrawChar(G, '_' ORTHOCGOARGVAR);
              }
            }
          }
          l = (I->CurLine - (lcount + skip_prompt)) & OrthoSaveLines;
          y = y + cOrthoLineHeight;
        }
      }

      OrthoDrawWizardPrompt(G ORTHOCGOARGVAR);

      if(SettingGetGlobal_b(G, cSetting_text) || I->SplashFlag) {
        Block *block;
        int active_tmp;
        block = SeqGetBlock(G);
        active_tmp = block->active;
        block->active = false;
        BlockRecursiveDraw(I->Blocks ORTHOCGOARGVAR);
        block->active = active_tmp;
      } else {
        BlockRecursiveDraw(I->Blocks ORTHOCGOARGVAR);
      }

      PRINTFD(G, FB_Ortho)
        " OrthoDoDraw: blocks drawn.\n" ENDFD;

      if(I->LoopFlag) {
	if (generate_shader_cgo){
	  CGOColor(orthoCGO, 1.f, 1.f, 1.f);

	  CGOBegin(orthoCGO, GL_TRIANGLE_STRIP);
	  CGOVertex(orthoCGO, I->LoopRect.left, I->LoopRect.bottom, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.left, I->LoopRect.top+1, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.left+1, I->LoopRect.bottom, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.left+1, I->LoopRect.top+1, 0.f);
	  CGOEnd(orthoCGO);
	  CGOBegin(orthoCGO, GL_TRIANGLE_STRIP);
	  CGOVertex(orthoCGO, I->LoopRect.left, I->LoopRect.top, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.left, I->LoopRect.top+1, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.right, I->LoopRect.top, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.right, I->LoopRect.top+1, 0.f);
	  CGOEnd(orthoCGO);
	  CGOBegin(orthoCGO, GL_TRIANGLE_STRIP);
	  CGOVertex(orthoCGO, I->LoopRect.right, I->LoopRect.bottom, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.right, I->LoopRect.top+1, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.right+1, I->LoopRect.bottom, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.right+1, I->LoopRect.top+1, 0.f);
	  CGOEnd(orthoCGO);
	  CGOBegin(orthoCGO, GL_TRIANGLE_STRIP);
	  CGOVertex(orthoCGO, I->LoopRect.left, I->LoopRect.bottom, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.left, I->LoopRect.bottom+1, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.right, I->LoopRect.bottom, 0.f);
	  CGOVertex(orthoCGO, I->LoopRect.right, I->LoopRect.bottom+1, 0.f);
	  CGOEnd(orthoCGO);
	} else {
	  glColor3f(1.0, 1.0, 1.0);
	  glBegin(GL_LINE_LOOP);
	  glVertex2i(I->LoopRect.left, I->LoopRect.top);
	  glVertex2i(I->LoopRect.right, I->LoopRect.top);
	  glVertex2i(I->LoopRect.right, I->LoopRect.bottom);
	  glVertex2i(I->LoopRect.left, I->LoopRect.bottom);
	  glVertex2i(I->LoopRect.left, I->LoopRect.top);
	  glEnd();
	}
      }


      /* BEGIN PROPRIETARY CODE SEGMENT (see disclaimer in "os_proprietary.h") */
#ifdef PYMOL_EVAL
      OrthoDrawEvalMessage(G ORTHOCGOARGVAR);
#endif
#ifdef PYMOL_BETA
      OrthoDrawBetaMessage(G);
#endif
#ifdef JYMOL_EVAL
      OrthoDrawEvalMessage(G);
#endif
#ifdef PYMOL_EDU
      OrthoDrawEduMessage(G);
#endif
#ifdef PYMOL_COLL
      OrthoDrawCollMessage(G);
#endif
#ifdef AXPYMOL_EVAL
      OrthoDrawAxMessage(G);
#endif

      /* END PROPRIETARY CODE SEGMENT */

      OrthoPopMatrix(G);

      if(Feedback(G, FB_OpenGL, FB_Debugging))
        PyMOLCheckOpenGLErr("OrthoDoDraw final checkpoint");

    }                           /* while */

  }

  if (generate_shader_cgo){
    int ok = true;

    /* This implements one shader for both text and solid polygons rendered for the orthoCGO */
    CGOStop(orthoCGO);
    {
      CGO *expandedCGO = CGOExpandDrawTextures(orthoCGO, 0);
      CHECKOK(ok, expandedCGO);
      if (ok)
	I->orthoCGO = CGOOptimizeScreenTexturesAndPolygons(expandedCGO, 0);
      CHECKOK(ok, I->orthoCGO);
      CGOFree(orthoCGO);
      CGOFree(expandedCGO);
      if (ok){
	CGOStop(I->orthoCGO);
	I->orthoCGO->use_shader = true;
      }
      
      while(origtimes--){
	switch (origtimes){
	case 1:
	  OrthoDrawBuffer(G, GL_BACK_LEFT);
	  break;
	case 0:
	  if(double_pump) {
	    OrthoDrawBuffer(G, GL_BACK_RIGHT);
	  } else
	    OrthoDrawBuffer(G, GL_BACK);
	  break;
	}
	OrthoPushMatrix(G);
	OrthoRenderCGO(G);
	OrthoPopMatrix(G);
      }
    }
  }

  I->DirtyFlag = false;
  PRINTFD(G, FB_Ortho)
    " OrthoDoDraw: leaving...\n" ENDFD;

}

void OrthoRenderCGO(PyMOLGlobals * G){
  COrtho *I = G->Ortho;
  if (I->orthoCGO) {
    SceneDrawImageOverlay(G, NULL);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    if (I->orthoCGO)
      CGORenderGL(I->orthoCGO, NULL, NULL, NULL, NULL, NULL);
    if (I->orthoFastCGO)
      CGORenderGL(I->orthoFastCGO, NULL, NULL, NULL, NULL, NULL);
    CShaderPrg_Disable(CShaderPrg_Get_Current_Shader(G));
    glEnable(GL_DEPTH_TEST);
  }
}

/*========================================================================*/

void OrthoDrawWizardPrompt(PyMOLGlobals * G ORTHOCGOARG)
{
  /* assumes PMGUI */

  COrtho *I = G->Ortho;

  char *vla, *p;
  int nLine;
  int x, y, xx;
  int nChar, c, ll;
  int maxLen;
  BlockRect rect;
  int prompt_mode = SettingGetGlobal_i(G, cSetting_wizard_prompt_mode);
  int gui_mode = SettingGetGlobal_b(G, cSetting_internal_gui_mode);
  float *text_color = I->WizardTextColor;
  float black[3] = { 0.0F, 0.0F, 0.0F };

  if(I->WizardPromptVLA && prompt_mode) {
    vla = I->WizardPromptVLA;

    if(gui_mode)
      text_color = black;
    nLine = UtilCountStringVLA(vla);
    if(nLine) {
      nChar = VLAGetSize(I->WizardPromptVLA);

      /* count max line length; it's strlen - X, 
       * where X is 4*n, where n is the number
       * of colors in the text label */

      maxLen = 0;
      p = vla;
      ll = 0;
      c = nChar;
      while(c > 0) {
        if(!*p) {
          if(maxLen < ll)
            maxLen = ll;
          ll = 0;
          p++;
          c--;
        } else if(((*p) == '\\') &&     /* color encoded */
                  (p[1] >= '0') && (p[1] <= '9') && (p[2] >= '0') && (p[2] <= '9') && (p[3] >= '0') && (p[3] <= '9')) { /* relying upon short-circuit logic to avoid overrun */
          p += 4;
          c -= 4;
        } else {
          ll++;
          p++;
          c--;
        }
      }

      /* determine the coordinates from which to draw the text;
       * need to make adjustments for the sequence viewer */

      rect.top = I->Height;
      if(I->HaveSeqViewer)
        if(!SettingGetGlobal_b(G, cSetting_seq_view_location)) {
          rect.top -= SeqGetHeight(G);
        }

      if(prompt_mode != 3) {
        rect.top -= cWizardTopMargin;
        rect.left = cWizardLeftMargin;
      } else {
        rect.top -= 1;
        rect.left = 1;
      }

      rect.bottom = rect.top - (nLine * cOrthoLineHeight + 2 * cWizardBorder) - 2;
      rect.right = rect.left + cOrthoCharWidth * maxLen + 2 * cWizardBorder + 1;

      if(prompt_mode == 1) {
	if (orthoCGO){
	  if(SettingGetGlobal_b(G, cSetting_internal_gui_mode)) {
	    CGOColor(orthoCGO, 1.0, 1.0F, 1.0F);
	  } else {
	    CGOColorv(orthoCGO, I->WizardBackColor);
	  }
	  CGOBegin(orthoCGO, GL_TRIANGLE_STRIP);
	  CGOVertex(orthoCGO, rect.right, rect.top, 0.f);
	  CGOVertex(orthoCGO, rect.right, rect.bottom, 0.f);
	  CGOVertex(orthoCGO, rect.left, rect.top, 0.f);
	  CGOVertex(orthoCGO, rect.left, rect.bottom, 0.f);
	  CGOEnd(orthoCGO);
	} else {
	  if(SettingGetGlobal_b(G, cSetting_internal_gui_mode)) {
	    glColor3f(1.0, 1.0F, 1.0F);
	  } else {
	    glColor3fv(I->WizardBackColor);
	  }
	  glBegin(GL_POLYGON);
	  glVertex2i(rect.right, rect.top);
	  glVertex2i(rect.right, rect.bottom);
	  glVertex2i(rect.left, rect.bottom);
	  glVertex2i(rect.left, rect.top);
	  glEnd();
	}
      }
      if (orthoCGO)
	CGOColorv(orthoCGO, text_color);
      else
	glColor3fv(text_color);

      x = rect.left + cWizardBorder;
      y = rect.top - (cWizardBorder + cOrthoLineHeight);

      vla = I->WizardPromptVLA;

      /* count max line length */

      TextSetColor(G, text_color);
      TextSetPos2i(G, x, y);
      xx = x;
      p = vla;
      ll = 0;
      c = nChar;
      /* set the char color, position the characters and draw the text */
      while(c > 0) {
        if(*p) {
          if((*p == '\\') && (*(p + 1)) && (*(p + 2)) && (*(p + 3))) {
            if(*(p + 1) == '-') {
              TextSetColor(G, text_color);
              p += 4;
              c -= 4;
            } else {
              TextSetColor3f(G, (*(p + 1) - '0') / 9.0F, (*(p + 2) - '0') / 9.0F,
                             (*(p + 3) - '0') / 9.0F);
              p += 4;
              c -= 4;
            }
            TextSetPos2i(G, xx, y);
          }
        }
        if(c--) {
          if(*p) {
            TextDrawChar(G, *p ORTHOCGOARGVAR);
            xx = xx + 8;
          }
          if(!*(p++)) {
            y = y - cOrthoLineHeight;
            xx = x;
            TextSetPos2i(G, x, y);
          }
        }
      }
    }
  }
}

static void OrthoLayoutPanel(PyMOLGlobals * G,
                             int m_top, int m_left, int m_bottom, int m_right)
{
  COrtho *I = G->Ortho;
  Block *block = NULL;

  int controlHeight = 20;
  int butModeHeight = ButModeGetHeight(G);
  int wizardHeight = I->WizardHeight;

  int controlBottom = m_bottom;
  int butModeBottom = controlBottom + controlHeight;
  int wizardBottom = butModeBottom + butModeHeight;
  int executiveBottom = wizardBottom + wizardHeight;

  int height = I->Height;

  if(SettingGetGlobal_b(G, cSetting_internal_gui)) {
    /* The Executive Block consists of the area in which object entries are rendered,
       if the wizard doesn't exist, then this region extends all the way down to the 
       top of the ButMode block */
    block = ExecutiveGetBlock(G);
    BlockSetMargin(block, m_top, m_left, executiveBottom, m_right);
    block->active = true;

    /* The Wizard Block is shown when a wizard is loaded, it is the area between the
       Executive Block and the ButMode Block, and is used for Wizard-related info/buttons */
    block = WizardGetBlock(G);
    BlockSetMargin(block, height - executiveBottom + 1, m_left, wizardBottom, m_right);
    block->active = false;

    /* The ButMode block shows info about which Mouse Mode, Selecting Mode, State info,
       and other info like frame rate. It is located under the Wizard Block, and above
       the Control Block */
    block = ButModeGetBlock(G);
    BlockSetMargin(block, height - wizardBottom + 1, m_left, butModeBottom, m_right);
    block->active = true;

    /* Controls are the Movie/Scene arrow buttons at the very bottom */
    block = ControlGetBlock(G);
    BlockSetMargin(block, height - butModeBottom + 1, m_left, controlBottom, m_right);
    block->active = true;
  } else {
    /* The Executive Block consists of the area in which object entries are rendered,
       if the wizard doesn't exist, then this region extends all the way down to the 
       top of the ButMode block */
    block = ExecutiveGetBlock(G);
    BlockSetMargin(block, m_right, m_bottom, m_right, m_bottom);
    block->active = false;

    /* The Wizard Block is shown when a wizard is loaded, it is the area between the
       Executive Block and the ButMode Block, and is used for Wizard-related info/buttons */
    block = WizardGetBlock(G);
    BlockSetMargin(block, m_right, m_bottom, m_right, m_bottom);
    block->active = false;

    /* The ButMode block shows info about which Mouse Mode, Selecting Mode, State info,
       and other info like frame rate. It is located under the Wizard Block, and above
       the Control Block */
    block = ButModeGetBlock(G);
    BlockSetMargin(block, m_right, m_bottom, m_right, m_bottom);
    block->active = false;

    /* Controls are the Movie/Scene arrow buttons at the very bottom */
    block = ControlGetBlock(G);
    BlockSetMargin(block, m_right, m_bottom, m_right, m_bottom);
    block->active = false;
  }
}


/*========================================================================*/
void OrthoReshape(PyMOLGlobals * G, int width, int height, int force)
{
  COrtho *I = G->Ortho;

  Block *block = NULL;
  int sceneBottom, sceneRight = 0;
  int textBottom = 0;
  int internal_gui_width;
  int internal_feedback;
  int sceneTop = 0;

  PRINTFD(G, FB_Ortho)
    " OrthoReshape-Debug: %d %d\n", width, height ENDFD;

  I->WrapXFlag = false;
  if(width > 0) {
    int stereo = SettingGetGlobal_i(G, cSetting_stereo);
    int stereo_mode = SettingGetGlobal_i(G, cSetting_stereo_mode);
    if (stereo){
      switch (stereo_mode) {
      case cStereo_geowall:
      case cStereo_dynamic:
	width = width / 2;
	I->WrapXFlag = true;
	break;
      }
    }
  }

  if((width != I->Width) || (height != I->Height) || force) {
    if(width < 0)
      width = I->Width;
    if(height < 0)
      height = I->Height;

    I->Height = height;
    I->Width = width;
    I->ShowLines = height / cOrthoLineHeight;

    textBottom += MovieGetPanelHeight(G);
    I->TextBottom = textBottom;

    internal_feedback = SettingGetGlobal_i(G, cSetting_internal_feedback);
    if(internal_feedback)
      sceneBottom =
        textBottom + (internal_feedback - 1) * cOrthoLineHeight + cOrthoBottomSceneMargin;
    else
      sceneBottom = textBottom;

    internal_gui_width = SettingGetGlobal_i(G, cSetting_internal_gui_width);
    if(!SettingGetGlobal_b(G, cSetting_internal_gui)) {
      internal_gui_width = 0;
      sceneRight = 0;
    } else {
      switch (SettingGetGlobal_i(G, cSetting_internal_gui_mode)) {
      case 2:
        sceneRight = 0;
        sceneBottom = 0;
        break;
      default:
        sceneRight = internal_gui_width;
        break;
      }
    }

    {
      int seqHeight;
      block = SeqGetBlock(G);
      block->active = true;

      /* reloate the sequence viewer as necessary */

      if(SettingGetGlobal_b(G, cSetting_seq_view_location)) {

        BlockSetMargin(block, height - sceneBottom - 10, 0, sceneBottom, sceneRight);
        if(block->fReshape)
          block->fReshape(block, width, height);
        seqHeight = SeqGetHeight(G);
        BlockSetMargin(block, height - sceneBottom - seqHeight, 0, sceneBottom,
                       sceneRight);
        if(!SettingGetGlobal_b(G, cSetting_seq_view_overlay)) {
          sceneBottom += seqHeight;
        }

      } else {

        BlockSetMargin(block, 0, 0, height - 10, sceneRight);
        if(block->fReshape)
          block->fReshape(block, width, height);
        seqHeight = SeqGetHeight(G);
        BlockSetMargin(block, 0, 0, height - seqHeight, sceneRight);
        if(!SettingGetGlobal_b(G, cSetting_seq_view_overlay)) {
          sceneTop = seqHeight;
        }
      }
    }

    OrthoLayoutPanel(G, 0, width - internal_gui_width, textBottom, 0);

    block = MovieGetBlock(G);
    BlockSetMargin(block, height - textBottom, 0, 0, 0);
    block->active = textBottom ? true : false;

    block = SceneGetBlock(G);
    BlockSetMargin(block, sceneTop, 0, sceneBottom, sceneRight);

    block = NULL;
    while(ListIterate(I->Blocks, block, next))
      if(block->fReshape) {
        block->fReshape(block, width, height);
      }

    WizardRefresh(G);           /* safe to call even if no wizard exists */
  }
  SceneInvalidateStencil(G);
  ShaderMgrResetUniformSet(G);
  OrthoInvalidateDoDraw(G);
  OrthoDirty(G);
}


/*========================================================================*/
void OrthoReshapeWizard(PyMOLGlobals * G, ov_size wizHeight)
{
  COrtho *I = G->Ortho;
  I->WizardHeight = wizHeight;

  if(SettingGetGlobal_b(G, cSetting_internal_gui) > 0.0) {
    Block *block;
    int internal_gui_width = SettingGetGlobal_i(G, cSetting_internal_gui_width);
    OrthoLayoutPanel(G, 0, I->Width - internal_gui_width, I->TextBottom, 0);

    block = ExecutiveGetBlock(G);
    block->fReshape(block, I->Width, I->Height);
    block = WizardGetBlock(G);
    block->fReshape(block, I->Width, I->Height);
    block->active = wizHeight ? true : false;
  }
}


/*========================================================================*/
Block *OrthoFindBlock(PyMOLGlobals * G, int x, int y)
{
  COrtho *I = G->Ortho;

  return (BlockRecursiveFind(I->Blocks, x, y));
}


/*========================================================================*/
int OrthoGetWrapClickSide(PyMOLGlobals * G)
{
  return G->Ortho->WrapClickSide;
}


/*========================================================================*/
int OrthoButton(PyMOLGlobals * G, int button, int state, int x, int y, int mod)
{
  COrtho *I = G->Ortho;
  Block *block = NULL;
  int handled = 0;

  PRINTFB(G, FB_Ortho, FB_Blather)
    "OrthoButton: button:%d, state=%d, x=%d, y=%d, mod=%d\n", 
    button,state,x,y,mod
    ENDFB(G);

  switch (button) {
  case 3:
  case 4:
    if((button != I->ActiveButton) && (I->ActiveButton>=0) && (I->ActiveButton<3)) {
      /* suppress wheel events when a button is already pushed */
      return 1;
    }
    block = SceneGetBlock(G);
    break;
  }

  if(I->WrapXFlag) {
    if(state == P_GLUT_DOWN) {
      x = get_wrap_x(x, NULL, G->Option->winX, &I->WrapClickSide);
    } else {
      x = get_wrap_x(x, &I->LastX, G->Option->winX, &I->WrapClickSide);
    }
  } else {
    I->WrapClickSide = 0;
  }

  OrthoRemoveSplash(G);
  OrthoRemoveAutoOverlay(G);
  I->X = x;
  I->Y = y;
  I->LastX = x;
  I->LastY = y;
  I->LastModifiers = mod;

  if(state == P_GLUT_DOWN) {
    I->ActiveButton = button;
    if(I->GrabbedBy) {
      if(I->GrabbedBy->inside)
        block = BlockRecursiveFind(I->GrabbedBy->inside, x, y);
      else
        block = I->GrabbedBy;
    } else if(!block)
      block = OrthoFindBlock(G, x, y);
    if(block) {
      I->ClickedIn = block;
      if(block->fClick) {
        handled = block->fClick(block, button, x, y, mod);
      }
    }
  } else if(state == P_GLUT_UP) {
    if(I->IssueViewportWhenReleased) {
      OrthoCommandIn(G, "viewport");
      I->IssueViewportWhenReleased = false;
    }
    
    if(I->GrabbedBy) {
      block = I->GrabbedBy;
      if(block->fRelease)
        handled = block->fRelease(block, button, x, y, mod);
      I->ClickedIn = NULL;
    }
    if(I->ClickedIn) {
      block = I->ClickedIn;
      if(block->fRelease)
        handled = block->fRelease(block, button, x, y, mod);
      I->ClickedIn = NULL;
    }
    I->ActiveButton = -1;
  }
  if (handled)
    OrthoInvalidateDoDraw(G);    
  return (handled);
}


/*========================================================================*/
int OrthoDrag(PyMOLGlobals * G, int x, int y, int mod)
{
  COrtho *I = G->Ortho;

  Block *block = NULL;
  int handled = 0;

  if(I->WrapXFlag) {
    x = get_wrap_x(x, &I->LastX, G->Option->winX, NULL);
  }

  I->LastX = x;
  I->LastY = y;
  I->LastModifiers = mod;

  I->X = x;
  I->Y = y;
  if(I->GrabbedBy) {
    block = I->GrabbedBy;
    if(block->fDrag)
      handled = block->fDrag(block, x, y, mod);
  } else if(I->ClickedIn) {
    block = I->ClickedIn;
    if(block->fDrag)
      handled = block->fDrag(block, x, y, mod);
  }
  if (handled && block!=SceneGetBlock(G))  // if user is not draging inside scene, then update OrthoCGO
    OrthoInvalidateDoDraw(G);
  return (handled);
}


/*========================================================================*/
void OrthoSplash(PyMOLGlobals * G)
{

/* BEGIN PROPRIETARY CODE SEGMENT (see disclaimer in "os_proprietary.h") */
#ifdef _PYMOL_IP_SPLASH
#include"OrthoIPSplash.h"
#else
  if(G->Option->incentive_product) {
#ifdef AXPYMOL_EVAL
    PRINTF
      " AxPyMOL(TM) Evaluation Product - Copyright (c) Schrodinger, LLC.\n \n"
      ENDF(G);
    PRINTF " This Executable Build integrates and extends Open-Source PyMOL " ENDF(G);
    PRINTF _PyMOL_VERSION ENDF(G);
    PRINTF ".\n" ENDF(G);
#else
    PRINTF " PyMOL(TM) Incentive Product - Copyright (c) Schrodinger, LLC.\n \n"
      ENDF(G);
    PRINTF " This Executable Build integrates and extends Open-Source PyMOL " ENDF(G);
    PRINTF _PyMOL_VERSION ENDF(G);
    PRINTF ".\n" ENDF(G);
#endif
  } else

/* END PROPRIETARY CODE SEGMENT */
  {
    /* Splash message for unrestricted access open-source versions... */
    PRINTF " PyMOL(TM) Molecular Graphics System, Version " ENDF(G);
    PRINTF _PyMOL_VERSION ENDF(G);
    PRINTF ".\n" ENDF(G);
    PRINTF " Copyright (c) Schrodinger, LLC.\n All Rights Reserved.\n \n"
      ENDF(G);

    PRINTF "    Created by Warren L. DeLano, Ph.D. \n \n" ENDF(G);

    /* PRINTF " Other Major Authors and Contributors:\n\n" ENDF(G);
     * PRINTF " Ralf W. Grosse-Kunstleve, Ph.D.\n \n" ENDF(G);
     * 
     * NOTICE: Enduring thanks to Ralf, but in point of fact, his
     * sglite module is no longer used by PyMOL, and thus we should
     * not mislead everyone by asserting otherwise... */

    PRINTF "    PyMOL is user-supported open-source software.  Although some versions\n"
      ENDF(G);
    PRINTF "    are freely available, PyMOL is not in the public domain.\n \n" ENDF(G);

    PRINTF "    If PyMOL is helpful in your work or study, then please volunteer \n"
      ENDF(G);
    PRINTF
      "    support for our ongoing efforts to create open and affordable scientific\n"
      ENDF(G);
    PRINTF
      "    software by purchasing a PyMOL Maintenance and/or Support subscription.\n\n"
      ENDF(G);

    PRINTF "    More information can be found at \"http://www.pymol.org\".\n \n" ENDF(G);

    PRINTF "    Enter \"help\" for a list of commands.\n" ENDF(G);
    PRINTF
      "    Enter \"help <command-name>\" for information on a specific command.\n\n"
      ENDF(G);

    PRINTF " Hit ESC anytime to toggle between text and graphics.\n\n" ENDF(G);
  }
#endif
}


/*========================================================================*/
int OrthoInit(PyMOLGlobals * G, int showSplash)
{
  COrtho *I = NULL;

  if((I = (G->Ortho = Calloc(COrtho, 1)))) {

    ListInit(I->Blocks);

    I->ActiveButton = -1;
    I->Pushed = 0;
    {
      int a;
      for(a = 0; a <= CMD_QUEUE_MASK; a++)
        I->cmdQueue[a] = QueueNew(G, 0x7FFF);   /* 32K ea. level for commands */
      I->cmdActiveQueue = I->cmdQueue[0];
      I->cmdNestLevel = 0;
    }
    I->feedback = QueueNew(G, 0x3FFFF); /* ~256K for output */
    I->deferred = NULL;
    I->RenderMode = 0;
    I->WrapXFlag = false;

    I->WizardBackColor[0] = 0.2F;
    I->WizardBackColor[1] = 0.2F;
    I->WizardBackColor[2] = 0.2F;
    I->WizardTextColor[0] = 0.2F;
    I->WizardTextColor[1] = 1.0F;
    I->WizardTextColor[2] = 0.2F;

    I->GrabbedBy = NULL;
    I->ClickedIn = NULL;
    I->DrawText = 1;
    I->HaveSeqViewer = false;
    I->TextColor[0] = 0.83F;
    I->TextColor[1] = 0.83F;
    I->TextColor[2] = 1.0;
    I->OverlayColor[0] = 1.0;
    I->OverlayColor[1] = 1.0;
    I->OverlayColor[2] = 1.0;
    I->CurLine = 1000;
    I->PromptChar = 0;
    I->CurChar = 0;
    I->CurLine = 0;
    I->AutoOverlayStopLine = 0;
    I->CursorChar = -1;
    I->HistoryLine = 0;
    I->HistoryView = 0;
    I->Line[I->CurLine & OrthoSaveLines][I->CurChar] = 0;
    I->WizardPromptVLA = NULL;
    I->SplashFlag = false;
    I->ShowLines = 1;
    I->Saved[0] = 0;
    I->DirtyFlag = true;
    I->ActiveGLBuffer = GL_NONE;
    I->LastDraw = UtilGetSeconds(G);
    I->DrawTime = 0.0;
    I->bg_texture_id = 0;
    I->bg_texture_needs_update = 0;
    I->bgCGO = NULL;
    I->bgWidth = I->bgHeight = 0;
    I->bgData = NULL;
    I->orthoCGO = NULL;
    if(showSplash) {
      OrthoSplash(G);
      I->SplashFlag = true;
    }
    /*  OrthoFeedbackIn(G," "); */
    I->CurLine++;

#ifndef _PYMOL_LIB
    /* prompt (and typing) should only be shown for PyMOL, not libpymol */
    strcpy(I->Prompt, "PyMOL>");
#endif
    strcpy(I->Line[I->CurLine], I->Prompt);
    I->CurChar = (I->PromptChar = strlen(I->Prompt));
    I->InputFlag = 1;

    /*printf("orthoNewLine: CC: %d CL:%d PC: %d IF:L %d\n",I->CurChar,I->CurLine,
       I->PromptChar,I->InputFlag); */

    PopInit(G);
    {
      int a;
      for(a = 0; a <= OrthoHistoryLines; a++)
        I->History[a][0] = 0;
    }

    return 1;
  } else {
    return 0;
  }
}


/*========================================================================*/
void OrthoFree(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;

  VLAFreeP(I->WizardPromptVLA);
  PopFree(G);
  {
    int a;
    I->cmdActiveQueue = NULL;
    for(a = 0; a <= CMD_QUEUE_MASK; a++) {
      QueueFree(I->cmdQueue[a]);
      I->cmdQueue[a] = NULL;
    }
  }
  QueueFree(I->feedback);
  I->feedback = NULL;
  if(I->deferred) {
    DeferredFree(I->deferred);
    I->deferred = NULL;
  }
  if (I->bgData){
    FreeP(I->bgData);
    I->bgData = NULL;
  }
  if (I->bgCGO){
    CGOFree(I->bgCGO);
  }
  FreeP(G->Ortho);
}


/*========================================================================*/
void OrthoPushMatrix(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;

  if(G->HaveGUI && G->ValidContext) {

    if(!I->Pushed) {
      glGetIntegerv(GL_VIEWPORT, I->ViewPort);
    }
    switch (I->RenderMode) {
    case 2:
      glViewport(I->ViewPort[0] + I->ViewPort[2], I->ViewPort[1],
                 I->ViewPort[2], I->ViewPort[3]);
      break;
    case 1:
    default:
      glViewport(I->ViewPort[0], I->ViewPort[1], I->ViewPort[2], I->ViewPort[3]);
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, I->ViewPort[2], 0, I->ViewPort[3], -100, 100);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(0.33F, 0.33F, 0.0F);   /* this generates better 
                                           rasterization on macs */

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_NORMALIZE);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);

#ifndef PURE_OPENGL_ES_2
    glShadeModel(SettingGetGlobal_b(G, cSetting_pick_shading) ? GL_FLAT : GL_SMOOTH);
#endif
    if(G->Option->multisample)
      glDisable(0x809D);        /* GL_MULTISAMPLE_ARB */
    I->Pushed++;
  }
  /*  glDisable(GL_ALPHA_TEST);
     glDisable(GL_CULL_FACE);
     glDisable(GL_POINT_SMOOTH); */

}


/*========================================================================*/
void OrthoPopMatrix(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  if(G->HaveGUI && G->ValidContext) {

    if(I->Pushed >= 0) {
      glViewport(I->ViewPort[0], I->ViewPort[1], I->ViewPort[2], I->ViewPort[3]);
      glPopMatrix();
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      I->Pushed--;
    }
  }
}

int OrthoGetPushed(PyMOLGlobals * G)
{
  return G->Ortho->Pushed;
}


/*========================================================================*/
void OrthoCommandIn(PyMOLGlobals * G, const char *buffer)
{
  COrtho *I = G->Ortho;
  if(I->cmdActiveQueue)
    QueueStrIn(I->cmdActiveQueue, buffer);
}

void OrthoCommandSetBusy(PyMOLGlobals * G, int busy){
  COrtho *I = G->Ortho;
  I->cmdActiveBusy = busy;
}

/*========================================================================*/
void OrthoPasteIn(PyMOLGlobals * G, const char *buffer)
{
  COrtho *I = G->Ortho;
  int curLine = I->CurLine & OrthoSaveLines;
  int execFlag = false;
  OrthoLineType buf2;

  if(I->InputFlag) {
    if(I->CursorChar >= 0) {
      strcpy(buf2, I->Line[curLine] + I->CursorChar);
      strcpy(I->Line[curLine] + I->CursorChar, buffer);
      I->CurChar = strlen(I->Line[curLine]);
      I->CursorChar = I->CurChar;
      while((I->Line[curLine][I->CurChar - 1] == 10)
            || (I->Line[curLine][I->CurChar - 1] == 13)) {
        execFlag = true;
        I->CurChar--;
        I->Line[curLine][I->CurChar] = 0;
        if(I->CurChar <= I->PromptChar)
          break;
      }
      if(!execFlag) {
        strcpy(I->Line[curLine] + I->CursorChar, buf2);
        I->CurChar = strlen(I->Line[curLine]);
      }
    } else {
      strcat(I->Line[curLine], buffer);
      I->CurChar = strlen(I->Line[curLine]);
      while((I->Line[curLine][I->CurChar - 1] == 10)
            || (I->Line[curLine][I->CurChar - 1] == 13)) {
        execFlag = true;
        I->CurChar--;
        I->Line[curLine][I->CurChar] = 0;
        if(I->CurChar <= I->PromptChar)
          break;
      }
    }
  } else {
    OrthoRestorePrompt(G);

    while((I->Line[curLine][I->CurChar - 1] == 10)
          || (I->Line[curLine][I->CurChar - 1] == 13)) {
      execFlag = true;
      I->CurChar--;
      I->Line[curLine][I->CurChar] = 0;
      if(I->CurChar <= I->PromptChar)
        break;
    }
  }
  if(execFlag) {
    printf("[%s]\n", I->Line[curLine]);
    OrthoParseCurrentLine(G);
  } else
    I->InputFlag = true;
}

void OrthoInvalidateDoDraw(PyMOLGlobals * G)
{
  COrtho *I = G->Ortho;
  if (I->orthoCGO){
    CGOFree(I->orthoCGO);
    I->orthoCGO = NULL;
    PyMOL_NeedRedisplay(G->PyMOL);
  }
}
