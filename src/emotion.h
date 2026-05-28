#pragma once
#include <stdint.h>
#include <string.h>

enum Emotion {
  EMOTION_IDLE = 0,
  EMOTION_THINKING,
  EMOTION_WORKING,
  EMOTION_BASH,
  EMOTION_READ,
  EMOTION_WRITE,
  EMOTION_SUCCESS,
  EMOTION_ERROR,
  EMOTION_SLEEPY,
  EMOTION_COUNT
};

// Infer emotion from a BLE promptTool field value.
inline Emotion inferEmotionFromTool(const char* toolName) {
  if (!toolName || toolName[0] == 0) return EMOTION_WORKING;
  if (strcmp(toolName, "Bash") == 0) return EMOTION_BASH;
  if (strcmp(toolName, "Read")  == 0 ||
      strcmp(toolName, "LS")    == 0 ||
      strcmp(toolName, "Grep")  == 0 ||
      strcmp(toolName, "Glob")  == 0) return EMOTION_READ;
  if (strcmp(toolName, "Write")        == 0 ||
      strcmp(toolName, "Edit")         == 0 ||
      strcmp(toolName, "TodoWrite")    == 0 ||
      strcmp(toolName, "NotebookEdit") == 0) return EMOTION_WRITE;
  if (strcmp(toolName, "Agent")        == 0) return EMOTION_THINKING;
  return EMOTION_WORKING;
}

class EmotionRenderer {
public:
  void init();
  void setEmotion(Emotion e, float intensity = 0.5f);
  void tick(uint32_t frameCount);
  void renderTo(void* tgt, int x, int y);

  void transitionTo(Emotion target, uint32_t durationMs);
  bool isTransitioning() const;

  Emotion getCurrent() const { return current; }

  static const int FACE_W = 50;
  static const int FACE_H = 50;

private:
  Emotion current     = EMOTION_IDLE;
  Emotion targetEmotion = EMOTION_IDLE;
  float   intensity   = 0.5f;
  uint32_t transitionStart    = 0;
  uint32_t transitionDuration = 0;
  bool     inTransition       = false;

  void drawIdle(int cx, int cy, uint32_t t);
  void drawThinking(int cx, int cy, uint32_t t);
  void drawWorking(int cx, int cy, uint32_t t);
  void drawBash(int cx, int cy, uint32_t t);
  void drawRead(int cx, int cy, uint32_t t);
  void drawWrite(int cx, int cy, uint32_t t);
  void drawSuccess(int cx, int cy, uint32_t t);
  void drawError(int cx, int cy, uint32_t t);
  void drawSleepy(int cx, int cy, uint32_t t);

  void drawFace(int cx, int cy, uint16_t faceColor, bool eyesOpen,
                int eyeOffsetX = 0, int eyeOffsetY = 0);
  void drawBlinking(int cx, int cy, uint32_t t, int blinkPeriod);
};
