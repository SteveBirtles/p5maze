#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

const int w = 1280;
const int h = 1024;
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
  std::vector<std::array<vertex, 4>> partials;
  bool visible;
  bool wall;
  vertex normal;
  vertex centre;
  quad(vertex _p0, vertex _p1, vertex _p2, vertex _p3, bool _wall) {
    vertices[0] = _p0;
    vertices[1] = _p1;
    vertices[2] = _p2;
    vertices[3] = _p3;
    wall = _wall;
  }
};

std::vector<quad> quads;

class Olc3d2 : public olc::PixelGameEngine {
 public:
  Olc3d2() { sAppName = "Olc3d2"; }

 public:
  bool OnUserCreate() override {
    std::srand(std::time(nullptr));

    for (int i = -20; i < 20; i++) {
      for (int j = -20; j < 20; j++) {
        float x1 = i * 50;
        float y1 = j * 50;
        float x2 = x1 + 50;
        float y2 = y1;
        float x4 = x1;
        float y4 = y1 + 50;
        float x3 = x1 + (x2 - x1) + (x4 - x1);
        float y3 = y1 + (y2 - y1) + (y4 - y1);

        quads.push_back(
            quad(vertex(x1, y1, 50.0f, -1), vertex(x2, y2, 50.0f, -1),
                 vertex(x3, y3, 50.0f, -1), vertex(x4, y4, 50.0f, -1), false));
      }
    }

    for (int i = 0; i < 100; i++) {
      float x1 =
          static_cast<float>(rand()) / static_cast<float>(RAND_MAX / w) - w / 2;
      float y1 =
          static_cast<float>(rand()) / static_cast<float>(RAND_MAX / w) - w / 2;
      float x2 = x1 + 50;
      float y2 = y1;
      float x4 = x1;
      float y4 = y1 + 50;
      float x3 = x1 + (x2 - x1) + (x4 - x1);
      float y3 = y1 + (y2 - y1) + (y4 - y1);

      quads.push_back(quad(vertex(x1, y1, 50.0f, 1), vertex(x2, y2, 50.0f, 0),
                           vertex(x2, y2, 0.0f, 3), vertex(x1, y1, 0.0f, 2),
                           true));

      quads.push_back(quad(vertex(x2, y2, 50.0f, 1), vertex(x3, y3, 50.0f, 0),
                           vertex(x3, y3, 0.0f, 3), vertex(x2, y2, 0.0f, 2),
                           true));

      quads.push_back(quad(vertex(x3, y3, 50.0f, 1), vertex(x4, y4, 50.0f, 0),
                           vertex(x4, y4, 0.0f, 3), vertex(x3, y3, 0.0f, 2),
                           true));

      quads.push_back(quad(vertex(x4, y4, 50.0f, 1), vertex(x1, y1, 50.0f, 0),
                           vertex(x1, y1, 0.0f, 3), vertex(x4, y4, 0.0f, 2),
                           true));
    }

    return true;
  }

  float angle = 0;
  float myX = 0;
  float myY = 0;

  bool OnUserUpdate(float fElapsedTime) override {
    if (GetKey(olc::Key::LEFT).bHeld) angle += 0.025;
    if (GetKey(olc::Key::RIGHT).bHeld) angle -= 0.025;

    if (GetKey(olc::Key::W).bHeld) {
      myX += std::sin(angle) * 5;
      myY += std::cos(angle) * 5;
    }
    if (GetKey(olc::Key::S).bHeld) {
      myX -= std::sin(angle) * 5;
      myY -= std::cos(angle) * 5;
    }
    if (GetKey(olc::Key::A).bHeld) {
      myX += std::cos(angle) * 5;
      myY += -std::sin(angle) * 5;
    }
    if (GetKey(olc::Key::D).bHeld) {
      myX -= std::cos(angle) * 5;
      myY -= -std::sin(angle) * 5;
    }

    float m[3][3] = {{std::cos(angle), -std::sin(angle), 0},
                     {std::sin(angle), std::cos(angle), 0},
                     {0, 0, 1}};

    for (auto& q : quads) {
      for (int i = 0; i < 4; i++) {
        q.adjusted[i].x = (q.vertices[i].x - myX) * m[0][0] +
                          (q.vertices[i].y - myY) * m[0][1] +
                          q.vertices[i].z * m[0][2];
        q.adjusted[i].y = (q.vertices[i].x - myX) * m[1][0] +
                          (q.vertices[i].y - myY) * m[1][1] +
                          q.vertices[i].z * m[1][2];
        q.adjusted[i].z = (q.vertices[i].x - myX) * m[2][0] +
                          (q.vertices[i].y - myY) * m[2][1] +
                          q.vertices[i].z * m[2][2];

        q.visible = true;
        q.partials.clear();
      }

      if (!q.wall) {
        int lostCornerCount = 0;

        for (int j = 0; j < 4; j++) {
          if (q.adjusted[j].y < omega) lostCornerCount++;
        }

        if (lostCornerCount > 0) {
          q.visible = false;

          if (lostCornerCount < 4) {
            int n = 0;
            float nY = 0;
            for (int j = 0; j < 4; j++) {
              if (q.adjusted[j].y > nY) {
                nY = q.adjusted[j].y;
                n = j;
              }
            }

            float x0 = q.adjusted[n].x;
            float y0 = q.adjusted[n].y;

            float x1 = q.adjusted[(n + 1) % 4].x;
            float y1 = q.adjusted[(n + 1) % 4].y;
            float d1 = std::sqrt(std::pow(x1 - x0, 2) + std::pow(y1 - y0, 2));
            float dx1 = (x1 - x0) / d1;
            float dy1 = (y1 - y0) / d1;

            float x2 = q.adjusted[(n + 3) % 4].x;
            float y2 = q.adjusted[(n + 3) % 4].y;
            float d2 = std::sqrt(std::pow(x2 - x0, 2) + std::pow(y2 - y0, 2));
            float dx2 = (x2 - x0) / d2;
            float dy2 = (y2 - y0) / d2;

            float alpha = 1, beta = 1;

            if (y1 >= y2) {
              alpha = 1 * d1;
              beta = 0.25 * d2;
            } else {
              alpha = 0.25 * d1;
              beta = 1 * d2;
            }

            q.partials.push_back(
                {vertex(x0, y0, q.adjusted[n].z, -1),
                 vertex(x0 + dx1 * alpha, y0 + dy1 * alpha, q.adjusted[n].z,
                        -1),
                 vertex(x0 + dx1 * alpha + dx2 * beta,
                        y0 + dy1 * alpha + dy2 * beta, q.adjusted[n].z, -1),
                 vertex(x0 + dx2 * beta, y0 + dy2 * beta, q.adjusted[n].z,
                        -1)});

          } /*else if (lostCornerCount == 1) {
            int n = 0;
            float nY = 0;
            for (int j = 0; j < 4; j++) {
              if (q.adjusted[j].y > nY) {
                nY = q.adjusted[j].y;
                n = j;
              }
            }

            float alpha = 0.25;
            float beta = 0.25;

            float x1 = q.adjusted[n].x;
            float y1 = q.adjusted[n].y;
            float x2 = x1 + (q.adjusted[(n + 1) % 4].x - x1) * alpha;
            float y2 = y1 + (q.adjusted[(n + 1) % 4].y - y1) * alpha;
            float x4 = x1 + (q.adjusted[(n + 3) % 4].x - x1) * beta;
            float y4 = y1 + (q.adjusted[(n + 3) % 4].y - y1) * beta;
            float x3 = x1 + (x2 - x1) + (x4 - x1);
            float y3 = y1 + (y2 - y1) + (y4 - y1);

            q.partials.push_back({vertex(x1, y1, q.adjusted[n].z, -1),
                                  vertex(x2, y2, q.adjusted[n].z, -1),
                                  vertex(x3, y3, q.adjusted[n].z, -1),
                                  vertex(x4, y4, q.adjusted[n].z, -1)});
          }*/

        } else {
          for (int i = 0; i < 4; i++) {
            if (q.adjusted[i].y < omega) {
              int pair = q.vertices[i].pair;

              if (q.adjusted[pair].y < omega) {
                q.visible = false;
                break;
              }

              float x0 = q.adjusted[i].x;
              float y0 = q.adjusted[i].y;
              float x1 = q.adjusted[pair].x;
              float y1 = q.adjusted[pair].y;

              float delta = (omega - y0) / (y1 - y0);

              q.adjusted[i].x = q.adjusted[i].x +
                                delta * (q.adjusted[pair].x - q.adjusted[i].x);

              q.adjusted[i].y = q.adjusted[i].y +
                                delta * (q.adjusted[pair].y - q.adjusted[i].y);
            }
          }
        }
      }

      float ax = q.adjusted[1].x - q.adjusted[0].x;
      float ay = q.adjusted[1].y - q.adjusted[0].y;
      float az = q.adjusted[1].z - q.adjusted[0].z;

      float bx = q.adjusted[3].x - q.adjusted[0].x;
      float by = q.adjusted[3].y - q.adjusted[0].y;
      float bz = q.adjusted[3].z - q.adjusted[0].z;

      float cx = q.adjusted[0].x + (q.adjusted[3].x - q.adjusted[0].x) / 2 +
                 (q.adjusted[1].x - q.adjusted[0].x) / 2;
      float cy = q.adjusted[0].y + (q.adjusted[3].y - q.adjusted[0].y) / 2 +
                 (q.adjusted[1].y - q.adjusted[0].y) / 2;
      float cz = q.adjusted[0].z + (q.adjusted[3].z - q.adjusted[0].z) / 2 +
                 (q.adjusted[1].z - q.adjusted[0].z) / 2;

      q.normal =
          vertex(ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx);
      q.centre = vertex(cx, cy, cz);

      float dotProduct = q.centre.x * q.normal.x + q.centre.y * q.normal.y +
                         q.centre.z * q.normal.z;

      if (dotProduct < 0) {
        q.visible = false;
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
      if (q.partials.size() > 0) {
        // FillCircle(-q.centre.x + w / 2, -q.centre.y + h / 2, 3, olc::YELLOW);

        for (auto p : q.partials) {
          DrawLine(-p[0].x + w / 2, -p[0].y + h / 2, -p[1].x + w / 2,
                   -p[1].y + h / 2, olc::YELLOW);
          DrawLine(-p[1].x + w / 2, -p[1].y + h / 2, -p[2].x + w / 2,
                   -p[2].y + h / 2, olc::YELLOW);
          DrawLine(-p[2].x + w / 2, -p[2].y + h / 2, -p[3].x + w / 2,
                   -p[3].y + h / 2, olc::YELLOW);
          DrawLine(-p[3].x + w / 2, -p[3].y + h / 2, -p[0].x + w / 2,
                   -p[0].y + h / 2, olc::YELLOW);
        }

      } else if (q.visible) {
        DrawLine(-q.adjusted[0].x + w / 2, -q.adjusted[0].y + h / 2,
                 -q.adjusted[1].x + w / 2, -q.adjusted[1].y + h / 2,
                 q.wall ? olc::GREEN : olc::DARK_CYAN);
        DrawLine(-q.adjusted[1].x + w / 2, -q.adjusted[1].y + h / 2,
                 -q.adjusted[2].x + w / 2, -q.adjusted[2].y + h / 2,
                 q.wall ? olc::GREEN : olc::DARK_CYAN);
        DrawLine(-q.adjusted[2].x + w / 2, -q.adjusted[2].y + h / 2,
                 -q.adjusted[3].x + w / 2, -q.adjusted[3].y + h / 2,
                 q.wall ? olc::GREEN : olc::DARK_CYAN);
        DrawLine(-q.adjusted[3].x + w / 2, -q.adjusted[3].y + h / 2,
                 -q.adjusted[0].x + w / 2, -q.adjusted[0].y + h / 2,
                 q.wall ? olc::GREEN : olc::DARK_CYAN);
        FillCircle(-q.centre.x + w / 2, -q.centre.y + h / 2, 3,
                   q.wall ? olc::GREEN : olc::DARK_CYAN);
      } else {
        DrawLine(-q.adjusted[0].x + w / 2, -q.adjusted[0].y + h / 2,
                 -q.adjusted[1].x + w / 2, -q.adjusted[1].y + h / 2,
                 q.wall ? olc::DARK_RED : olc::DARK_MAGENTA);
        DrawLine(-q.adjusted[1].x + w / 2, -q.adjusted[1].y + h / 2,
                 -q.adjusted[2].x + w / 2, -q.adjusted[2].y + h / 2,
                 q.wall ? olc::DARK_RED : olc::DARK_MAGENTA);
        DrawLine(-q.adjusted[2].x + w / 2, -q.adjusted[2].y + h / 2,
                 -q.adjusted[3].x + w / 2, -q.adjusted[3].y + h / 2,
                 q.wall ? olc::DARK_RED : olc::DARK_MAGENTA);
        DrawLine(-q.adjusted[3].x + w / 2, -q.adjusted[3].y + h / 2,
                 -q.adjusted[0].x + w / 2, -q.adjusted[0].y + h / 2,
                 q.wall ? olc::DARK_RED : olc::DARK_MAGENTA);
      }
    }

    return true;
  }
};

int main() {
  Olc3d2 demo;
  if (demo.Construct(w, h, 1, 1)) demo.Start();
  return 0;
}