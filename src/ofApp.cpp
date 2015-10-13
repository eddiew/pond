#include "ofApp.h"
#include <algorithm>

//--------------------------------------------------------------
void ofApp::setup(){

	ofSetVerticalSync(true);
	ofSetCircleResolution(32);
	ofBackground(0, 0, 0);

	ofEnableSmoothing();

	// Set up sound input & fft
	fft.stream.setDeviceID(3);
	fft.setUseNormalization(false);
	fft.setup(1024, OF_FFT_WINDOW_HAMMING, OF_FFT_FFTW, 512, 44100);

	// Initialize camera position
	camera.setFov(135);
	ofVec3f defaultCameraPosition(X/2, Y/2, X/4);
	ofVec3f cameraPosition = defaultCameraPosition;
	camera.setPosition(cameraPosition);
	ofVec3f cameraTarget(X/2, Y/2, 0);
	camera.setTarget(cameraTarget);
	camera.setDistance(X/4);
}

// color temperature (in kelvin) to RGB
color kToRgb(float k) {
	color rgb;
	float hk = k/100;
	if (hk <= 66) {
		rgb.r = 1;
		rgb.g = 0.390081 * log(hk) - 0.631841;
		if (hk <= 19)
			rgb.b = 0;
		else
			rgb.b = 0.543207 * log(hk - 10) - 1.196254;
	}
	else {
		rgb.r = 1.292936 * pow(hk - 60, -0.1332047592);
		rgb.g = 1.129891 * pow(hk - 60, -0.0755148492);
		rgb.b = 1;
	}
	return rgb;
}

int colorTemps[N_OCTAVES] = {
	1500, 2500, 3750, 5500, 7500, 10000, 20000, 50000, 100000, 400000
};

color getOctaveColor(int octave) {
	return kToRgb(colorTemps[octave]);
}

float getOctaveYPosition(float octave) {
	return Y * (octave + 1) / (N_OCTAVES + 1);
}

//--------------------------------------------------------------
void ofApp::update(){
	// Begin sound processing

	fft.update();

	float t = ofGetElapsedTimef();

	// Get power for each octave
	float volumes[N_OCTAVES] = {0};
	color netColor = {0, 0, 0};
	for (int i = 0; i < N_OCTAVES; i++) {
		int start = 1L << (i-1);
		int end = 1L << i;

		for (int j = start + 1; j <= end; j++)
			volumes[i] += fft.getBins()[j];
		netColor = netColor + getOctaveColor(i) * max(0., volumes[i] - NOISE_FLOOR);
	}
	netColor = netColor * (1.f / max(1.f, max(netColor.r, max(netColor.g, netColor.b))));
	smoothColor = smoothColor * 0.75 + netColor * 0.25;

	// Get smoothed volumes
	bool newly_active[N_OCTAVES];
	bool newly_inactive[N_OCTAVES];
	for (int i = 0; i < N_OCTAVES; i++) {
		float f = 0.75;
		smoothVolumes[i] = smoothVolumes[i] * f + (1.f-f) * volumes[i];
		float ff = 0.99;
		smootherVolumes[i] = smootherVolumes[i] * ff + (1.f-ff) * volumes[i];

		newly_active[i] = (smoothVolumes[i] - NOISE_FLOOR > smootherVolumes[i] * 1.15 &&
			!active[i]);

		newly_inactive[i] = (smoothVolumes[i] - NOISE_FLOOR < smootherVolumes[i] * 0.9 && active[i]);
	}

	// End sound processing

	for (int i = 0; i < N_OCTAVES; i++) {
		// If channel is newly active, add a drop
		if (newly_active[i]) {
			drop *d = new drop;
			d->x = ofRandom(0, X);
			d->y = getOctaveYPosition((float)i + ofRandom(0, 0.5) - ofRandom(0, 0.5));
			d->f = 1.5 + (float)i / 2;
			d->start = t;
			d->stop = FLT_MAX;
			d->c = getOctaveColor(i);
			d->octave = i;
			drops.insert(d);
			octaveDrops[i] = d;
			active[i] = true;
		}
		// If channel is newly inactive, stop its drop
		else if (newly_inactive[i]) {
			octaveDrops[i]->stop = t;
			active[i] = false;
		}
		// Adjust drop amplitude based on channel volume while live
		if (active[i])
			octaveDrops[i]->a = smoothVolumes[i];
	}

	// Prune set of drops
	// drops.erase(remove_if(drops.begin(), drops.end(), [t](const drop const*d) {
	// 	return ((t - d->start) > 10);
	// }), drops.end());

	// :( this code sucks
	vector<drop *> dropsToDelete;
	for (drop *d : drops) {
		if ((t - d->stop) > sqrt(X*X+Y*Y) / WAVE_SPEED + 1) {
			dropsToDelete.push_back(d);
		}
	}
	for (drop *d : dropsToDelete) {
		drops.erase(d);
	}

}

//--------------------------------------------------------------
void ofApp::draw(){

	vector<float> samples = fft.getAudio();
	vector<float> bins = fft.getBins();

	camera.begin();

	float t = ofGetElapsedTimef();

	memset(heights, 0, sizeof(heights));
	memset(colors, 0, sizeof(colors));

	// TODO: make this more efficient
	ofFill();
	for (int x = 0; x < X; x++) {
		for (int y = 0; y < Y; y++) {
			for (drop *d : drops) {
				float waveEnd = (M_PI / 2) / d->f;

				// quick bounding box check to improve performance
				float min_dist = max(abs(d->x - x), abs(d->y - y));
				float min_time = min_dist / WAVE_SPEED;
				if (t - d->start < min_time)
					continue;
				float max_dist = abs(d->x - x) + abs(d->y - y);
				float max_time = max_dist + waveEnd;
				if (t - d->stop > max_time)
					continue;

				// Compute drop amplitude for this x, y
				float dist = ofDist(x, y, d->x, d->y);
				float wt = t - (dist / WAVE_SPEED);
				float waveTime = wt - d->start;
				float wavePast = wt - d->stop;
				if (waveTime < 0 || wavePast > waveEnd)
					continue;
				float power = d->a;
				if (wavePast > 0)
					power *= pow(0.01, wavePast);

				float amplitude = sqrt(power / (0.25 + dist));

				// Add displacement and color from this drop
				heights[x][y] += 10 * amplitude * sin(2 * M_PI * d->f * waveTime) / d->f;
				colors[x][y] += d->c * amplitude;
			}
		}
	}

	// Draw dots
	for (int x = 0; x < X; x++) {
		for (int y = 0; y < Y; y++) {
			// Scale color if clipped
			float maxChannelValue = max(max(colors[x][y].r, colors[x][y].g), colors[x][y].b);
			color dotColor = colors[x][y] * (1.f / max(1.f, maxChannelValue));

			ofSetColor(max(31.f, dotColor.r * 255),
					   max(31.f, dotColor.g * 255),
					   max(31.f, dotColor.b * 255));
			ofCircle(x, y, heights[x][y], 0.2);
		}
	}

	// Draw octaves + waveform + instructions
	if (drawDetails) {
		for (int i = 0; i < N_OCTAVES; i++) {
			color c = getOctaveColor(i);
			ofSetColor(c.r * 255, c.g * 255, c.b * 255);
			ofNoFill();
			ofCircle(-10, getOctaveYPosition(i), 0, 0.2 + 5 * smootherVolumes[i]);
			ofFill();
			ofCircle(-10, getOctaveYPosition(i), 0, 0.2 + 5 * smoothVolumes[i]);
		}

		ofSetLineWidth(0.15);
		ofSetColor(max(127.f, smoothColor.r * 255),
				   max(127.f, smoothColor.g * 255),
				   max(127.f, smoothColor.b * 255));
		ofNoFill();
		ofBeginShape();
		for (uint i = 0; i < samples.size(); i++)
			ofVertex((float)X * (i+1) / (samples.size()+1), -10 + 10 * samples[i]);
		ofEndShape();

	}

	camera.end();
	if (drawDetails)
		ofDrawBitmapString("Press <space> to toggle details, <e> to freeze mic imput, and <s> to resume it", 50, 50);
}

//--------------------------------------------------------------
void ofApp::audioIn(float * input, int bufferSize, int nChannels){
}

//--------------------------------------------------------------
void ofApp::keyPressed  (int key){
	if( key == 's' ){
		fft.stream.start();
	}

	if( key == 'e' ){
		fft.stream.stop();
	}

	if( key == ' ' ){
		drawDetails = !drawDetails;
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){

}

