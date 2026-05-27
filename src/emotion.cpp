#include "emotion.h"
#include "hal.h"
#include <math.h>

extern TFT_eSprite spr;

const uint16_t FACE_WHITE  = 0xFFFF;
const uint16_t FACE_YELLOW = 0xFFE0;
const uint16_t FACE_RED    = 0xF800;
const uint16_t FACE_BLUE   = 0x041F;
const uint16_t FACE_GREEN  = 0x07E0;
const uint16_t FACE_CYAN   = 0x07FF;
const uint16_t FACE_GRAY   = 0x8410;
const uint16_t EYE_BLACK   = 0x0000;

void EmotionRenderer::init() {
  current       = EMOTION_IDLE;
  targetEmotion = EMOTION_IDLE;
  intensity     = 0.5f;
  inTransition  = false;
}

void EmotionRenderer::setEmotion(Emotion e, float intens) {
  if (e == current) { intensity = intens; return; }
  transitionTo(e, 500);
  intensity = intens;
}

void EmotionRenderer::transitionTo(Emotion target, uint32_t durMs) {
  targetEmotion       = target;
  transitionDuration  = durMs;
  transitionStart     = millis();
  inTransition        = true;
}

bool EmotionRenderer::isTransitioning() const {
  return inTransition && (millis() - transitionStart < transitionDuration);
}

void EmotionRenderer::tick(uint32_t /*frameCount*/) {
  if (!inTransition) return;
  if (millis() - transitionStart >= transitionDuration) {
    current      = targetEmotion;
    inTransition = false;
  }
}

void EmotionRenderer::renderTo(void* /*tgt*/, int x, int y) {
  uint32_t t = millis();
  int cx = x + FACE_W / 2;
  int cy = y + FACE_H / 2;
  switch (current) {
    case EMOTION_IDLE:     drawIdle(cx, cy, t);     break;
    case EMOTION_THINKING: drawThinking(cx, cy, t); break;
    case EMOTION_WORKING:  drawWorking(cx, cy, t);  break;
    case EMOTION_BASH:     drawBash(cx, cy, t);     break;
    case EMOTION_READ:     drawRead(cx, cy, t);     break;
    case EMOTION_WRITE:    drawWrite(cx, cy, t);    break;
    case EMOTION_SUCCESS:  drawSuccess(cx, cy, t);  break;
    case EMOTION_ERROR:    drawError(cx, cy, t);    break;
    case EMOTION_SLEEPY:   drawSleepy(cx, cy, t);   break;
    default:               drawIdle(cx, cy, t);     break;
  }
}

void EmotionRenderer::drawFace(int cx, int cy, uint16_t faceColor,
                                bool eyesOpen, int eyeOffsetX, int eyeOffsetY) {
  spr.fillCircle(cx, cy, 25, faceColor);
  int leftEyeX  = cx - 8 + eyeOffsetX;
  int rightEyeX = cx + 8 + eyeOffsetX;
  int eyeY      = cy - 5 + eyeOffsetY;
  if (eyesOpen) {
    spr.fillCircle(leftEyeX,  eyeY, 4, EYE_BLACK);
    spr.fillCircle(rightEyeX, eyeY, 4, EYE_BLACK);
  } else {
    spr.fillRect(leftEyeX  - 4, eyeY - 1, 8, 2, EYE_BLACK);
    spr.fillRect(rightEyeX - 4, eyeY - 1, 8, 2, EYE_BLACK);
  }
}

void EmotionRenderer::drawBlinking(int cx, int cy, uint32_t t, int blinkPeriod) {
  bool eyesOpen = (t % blinkPeriod) < (uint32_t)(blinkPeriod * 0.9f);
  drawFace(cx, cy, FACE_WHITE, eyesOpen);
  int mouthY = cy + 10;
  for (int i = -6; i <= 6; i++) {
    int py = mouthY + (int)(sqrt(36 - i * i) * 0.4f);
    spr.drawPixel(cx + i, py, EYE_BLACK);
  }
}

void EmotionRenderer::drawIdle(int cx, int cy, uint32_t t) {
  int sway = (int)(sin(t / 2000.0f) * 2 * intensity);
  cx += sway;
  drawBlinking(cx, cy, t, 3000);
  int mouthY = cy + 10;
  for (int i = -6; i <= 6; i++) {
    int py = mouthY + (int)(sqrt(36 - i * i) * 0.4f);
    spr.drawPixel(cx + i, py, EYE_BLACK);
  }
}

void EmotionRenderer::drawThinking(int cx, int cy, uint32_t t) {
  float angle     = (t % 2000) / 2000.0f * TWO_PI;
  int eyeOffsetX  = (int)(cos(angle) * 3 * intensity);
  int eyeOffsetY  = (int)(sin(angle) * 2 * intensity);
  drawFace(cx, cy, FACE_WHITE, true, eyeOffsetX, eyeOffsetY);
  spr.drawLine(cx - 8, cy + 10, cx + 8, cy + 10, EYE_BLACK);
}

void EmotionRenderer::drawWorking(int cx, int cy, uint32_t t) {
  drawFace(cx, cy, FACE_WHITE, true);
  spr.fillRect(cx - 12, cy - 12, 6, 2, EYE_BLACK);
  spr.fillRect(cx +  6, cy - 12, 6, 2, EYE_BLACK);
  spr.drawLine(cx - 6, cy + 10, cx + 6, cy + 10, EYE_BLACK);
  int sweatCount = (int)(intensity * 3);
  for (int i = 0; i < sweatCount; i++) {
    int sx = cx + 28 + i * 6;
    int sy = cy - 10 + (t / 200) % 20;
    spr.fillCircle(sx, sy, 2, FACE_CYAN);
  }
}

void EmotionRenderer::drawBash(int cx, int cy, uint32_t t) {
  int bounceY  = (int)(abs((int)((t % 400) - 200)) / 20.0f * intensity);
  drawFace(cx, cy - bounceY, FACE_WHITE, true);
  int mouthY   = cy + 10 - bounceY;
  bool mOpen   = (t % 400) < 200;
  if (mOpen) spr.fillCircle(cx, mouthY, 4, EYE_BLACK);
  else       spr.drawLine(cx - 4, mouthY, cx + 4, mouthY, EYE_BLACK);
  int footX = cx - 10 + ((t % 400) < 200 ? -5 : 5);
  spr.fillRect(footX, cy + 35, 6, 3, FACE_GRAY);
}

void EmotionRenderer::drawRead(int cx, int cy, uint32_t t) {
  int scanOffset = (int)(((t % 1500) - 750) / 250.0f * intensity);
  drawFace(cx, cy, FACE_WHITE, true, scanOffset, 0);
  int mouthY = cy + 10;
  for (int i = -5; i <= 5; i++) {
    int py = mouthY + (int)(sqrt(25 - i * i) * 0.3f);
    spr.drawPixel(cx + i, py, EYE_BLACK);
  }
  spr.fillRect(cx - 10, cy + 30, 20, 15, FACE_GRAY);
  spr.fillRect(cx -  8, cy + 33, 16,  2, EYE_BLACK);
  spr.fillRect(cx -  8, cy + 37, 10,  2, EYE_BLACK);
}

void EmotionRenderer::drawWrite(int cx, int cy, uint32_t t) {
  drawFace(cx, cy, FACE_WHITE, true);
  spr.drawLine(cx - 4, cy + 10, cx + 4, cy + 10, EYE_BLACK);
  int penY = cy + 30 + (t % 600) / 100;
  if (penY > cy + 45) penY = cy + 30;
  spr.drawLine(cx + 20, penY, cx + 25, penY - 8, FACE_GRAY);
  int lineLen = (t % 600) / 30;
  spr.fillRect(cx - 10, cy + 50, lineLen, 2, EYE_BLACK);
}

void EmotionRenderer::drawSuccess(int cx, int cy, uint32_t t) {
  int bounce = abs((int)((t % 500) - 250)) / 15;
  drawFace(cx, cy - bounce, FACE_YELLOW, true);
  spr.fillCircle(cx, cy + 8 - bounce, 6, EYE_BLACK);
  if ((t % 1000) < 500) {
    spr.fillTriangle(cx - 30, cy - 20, cx - 25, cy - 30, cx - 20, cy - 20, FACE_YELLOW);
    spr.fillTriangle(cx + 20, cy - 20, cx + 25, cy - 30, cx + 30, cy - 20, FACE_YELLOW);
  }
}

void EmotionRenderer::drawError(int cx, int cy, uint32_t t) {
  drawFace(cx, cy, FACE_WHITE, true);
  for (int i = 0; i < 3; i++) {
    float sa = ((t / 100 + i * 120) % 360) * DEG_TO_RAD;
    spr.fillCircle(cx - 8 + (int)(cos(sa) * 3), cy - 5 + (int)(sin(sa) * 3), 1, EYE_BLACK);
    spr.fillCircle(cx + 8 + (int)(cos(sa) * 3), cy - 5 + (int)(sin(sa) * 3), 1, EYE_BLACK);
  }
  spr.fillCircle(cx, cy + 12, 4, EYE_BLACK);
}

void EmotionRenderer::drawSleepy(int cx, int cy, uint32_t t) {
  float eyelid = 0.5f + 0.3f * sin(t / 1000.0f);
  spr.fillCircle(cx, cy, 25, FACE_WHITE);
  int eyeY = cy - 5;
  spr.fillCircle(cx - 8, eyeY, 4, EYE_BLACK);
  spr.fillCircle(cx + 8, eyeY, 4, EYE_BLACK);
  int lidH = (int)(eyelid * 6);
  spr.fillRect(cx - 12, eyeY - 4, 8, lidH, FACE_WHITE);
  spr.fillRect(cx +  4, eyeY - 4, 8, lidH, FACE_WHITE);
  bool yawning = (t % 4000) < 1000;
  int mouthY = cy + 10;
  if (yawning) {
    spr.fillCircle(cx, mouthY + 2, 5, EYE_BLACK);
  } else {
    for (int i = -5; i <= 5; i++) {
      int py = mouthY + (int)(sqrt(25 - i * i) * 0.3f);
      spr.drawPixel(cx + i, py, EYE_BLACK);
    }
  }
  int zPhase = (t / 1000) % 3;
  for (int i = 0; i <= zPhase; i++) {
    spr.setTextColor(FACE_GRAY, FACE_GRAY);
    spr.setCursor(cx + 30 + i * 8, cy - 10 - i * 10);
    spr.print("z");
  }
}
