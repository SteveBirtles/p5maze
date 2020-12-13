#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

const int w = 1024;
const int h = 768;
const float omega = 10;

struct vertex {
  float x;
  float y;
  float z;
  int pair;
  vertex() {}
  vertex(float _x, float _y, float _z, int _pair = 0) {
    x = _x;
    y = _y;
    z = _z;
    pair = _pair;
  }
};

struct quad {
  std::array<vertex, 4> vertices;
  std::array<vertex, 4> adjusted;
  quad(vertex _p0, vertex _p1, vertex _p2, vertex _p3) {
    vertices[0] = _p0;
    vertices[1] = _p1;
    vertices[2] = _p2;
    vertices[3] = _p3;
  }
};

std::vector<quad> quads;

class Olc3d2 : public olc::PixelGameEngine {
 public:
  Olc3d2() { sAppName = "Olc3d2"; }

 public:
  bool OnUserCreate() override {
    quads.push_back(
        quad(vertex(150.0f, 50.0f, 50.0f, 1), vertex(50.0f, 150.0, 50.0f, 0),
             vertex(50.0f, 150.0f, 0.0f, 3), vertex(150.0f, 50.0f, 0.0f, 2)));

    return true;
  }

  float t = 0;

  bool OnUserUpdate(float fElapsedTime) override {
    t++;

    for (auto& q : quads) {
      for (int i = 0; i < 4; i++) {
        q.adjusted[i].x = q.vertices[i].x;
        q.adjusted[i].y = q.vertices[i].y + t;
        q.adjusted[i].z = q.vertices[i].z;

        if (q.adjusted[i].y < omega) {
        }
      }
    }

    Clear(olc::VERY_DARK_BLUE);

    DrawLine(w / 2, 0, w / 2, h, olc::BLUE);
    DrawLine(0, h / 2, w, h / 2, olc::BLUE);
    for (int i = 40; i < w / 2; i += 40) {
      DrawLine(w / 2 + i, 0, w / 2 + i, h, olc::DARK_BLUE);
      DrawLine(w / 2 - i, 0, w / 2 - i, h, olc::DARK_BLUE);
      if (i < h / 2) {
        DrawLine(0, h / 2 + i, w, h / 2 + i, olc::DARK_BLUE);
        DrawLine(0, h / 2 - i, w, h / 2 - i, olc::DARK_BLUE);
      }
    }

    DrawLine(0, h / 2 - omega, w, h / 2 - omega, olc::YELLOW);

    for (auto q : quads) {
      DrawLine(q.adjusted[0].x, q.adjusted[0].y, q.adjusted[1].x,
               q.adjusted[1].y, olc::RED);
      DrawLine(q.adjusted[1].x, q.adjusted[1].y, q.adjusted[2].x,
               q.adjusted[2].y, olc::RED);
      DrawLine(q.adjusted[2].x, q.adjusted[2].y, q.adjusted[3].x,
               q.adjusted[3].y, olc::RED);
      DrawLine(q.adjusted[3].x, q.adjusted[3].y, q.adjusted[0].x,
               q.adjusted[0].y, olc::RED);
    }

    return true;
  }
};

int main() {
  Olc3d2 demo;
  if (demo.Construct(w, h, 1, 1)) demo.Start();
  return 0;
}