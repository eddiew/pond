#pragma once

#include "ofMain.h"
#include "ofxEasyFft.h"
#include <unordered_set>

#define X 128
#define Y 128

#define WAVE_SPEED 30

#define N_OCTAVES 10

#define HISTORY_SIZE 16 * 60

#define NOISE_FLOOR 0.05

struct color {
	float r, g, b; // values in 0-1 range.

	void operator +=(const color &c) {
		r += c.r;
		g += c.g;
		b += c.b;
	}

	color operator +(const color &c) {
		color newColor;
		newColor.r = r + c.r;
		newColor.g = g + c.g;
		newColor.b = b + c.b;
		return newColor;
	}

	color operator *(const double s) {
		color newColor;
		newColor.r = r*s;
		newColor.g = g*s;
		newColor.b = b*s;
		return newColor;
	}
};

struct drop {
	float a, f;
	float x, y;
	float start, stop;
	color c;
	int octave;
};

namespace std {
	template<>
	struct hash<drop> {

		std::size_t operator()(const drop& d) const {
			using std::size_t;
			using std::hash;
			using std::string;

		    size_t const h0 (hash<float>()(d.a) );
		    size_t const h1 (hash<float>()(d.f) );
		    size_t const h2 (hash<float>()(d.x) );
		    size_t const h3 (hash<float>()(d.y) );
		    size_t const h4 (hash<float>()(d.start) );
		    size_t const h5 (hash<float>()(d.stop) );
		    return h0 ^ (h1 << 1) ^ (h2 << 2) ^ (h3 << 3) ^ (h4 << 4) ^ (h5 << 5);
		}
	};
}

class ofApp : public ofBaseApp{
private:
	void drawWindow(vector<float>& channel, int x, int y, int w, int h);

	public:

		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

		void audioIn(float * input, int bufferSize, int nChannels);

		ofxEasyFft fft;
		float smoothVolumes[N_OCTAVES] = {NOISE_FLOOR};
		float smootherVolumes[N_OCTAVES] = {NOISE_FLOOR};
		bool active[N_OCTAVES] = {false};
		color smoothColor;

		bool drawDetails = true;

		drop *octaveDrops[N_OCTAVES];

		float heights[X][Y];
		color colors[X][Y];

		ofEasyCam camera;
		ofVec3f cameraPosition;

		unordered_set<drop*> drops;
};
