#include "Controller.h"

using namespace std;
using namespace ci;
using namespace ci::app;

class KinectDepthRangeApp : public ci::app::AppBasic {
public:

	void prepareSettings(ci::app::AppBasic::Settings* settings) override;
	
	void setup() override;
	void update() override;
	void draw() override;

	void keyDown(ci::app::KeyEvent event) override;

private:

	Controller::Ref mController;
};

void KinectDepthRangeApp::prepareSettings(Settings* settings)
{
	settings->prepareWindow(ci::app::Window::Format().size(1024, 768).title("ITP FoS Sketch"));
	settings->setFrameRate(60.0f);
}

void KinectDepthRangeApp::setup()
{
	// Create controller:
	mController = Controller::create( this, "controller" );
}

void KinectDepthRangeApp::update()
{
	// Update controller:
	mController->update();
}

void KinectDepthRangeApp::draw()
{
	// Prepare context:
	gl::viewport(getWindowSize());
	gl::clear();
	gl::setMatricesWindow(getWindowSize());
	gl::enableAlphaBlending();
	// Draw controller:
	mController->draw();
}

void KinectDepthRangeApp::keyDown(ci::app::KeyEvent event)
{
	if (event.getChar() == ' ') {
		mController->popDirectiveTop(); // todo just testing
	}
}

CINDER_APP_BASIC(KinectDepthRangeApp, RendererGl)
