#include "cinder/app/AppBasic.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/Utilities.h"
#include "cinder/params/Params.h"

#include "Kinect2.h"

using namespace std;
using namespace ci;
using namespace ci::app;

class KinectDepthRangeApp : public ci::app::AppBasic {
public:

	void prepareSettings(ci::app::AppBasic::Settings* settings) override;
	
	void setup() override;
	void update() override;
	void draw() override;

private:

	long long					mTimeStamp;
	long long					mTimeStampPrev;

	Kinect2::DeviceRef			mDevice;

	ci::Channel8u				mChannelBody;
	ci::Surface8u				mSurfaceColor;
	ci::Channel16u				mChannelDepth;
	ci::Surface32f				mSurfaceLookup;

	ci::gl::TextureRef			mTextureBody;
	ci::gl::TextureRef			mTextureColor;
	ci::gl::TextureRef			mTextureDepth;
	ci::gl::TextureRef			mTextureLookup;

	ci::gl::GlslProgRef			mGlslProg;
};

void KinectDepthRangeApp::prepareSettings(Settings* settings)
{
	settings->prepareWindow(ci::app::Window::Format().size(1024, 768).title("ITP FoS Sketch"));
	settings->setFrameRate(60.0f);
}

void KinectDepthRangeApp::setup()
{
	// Enable texture mode:
	gl::enable(GL_TEXTURE_2D);
	// Initialize timestamp:
	mTimeStamp = 0L;
	mTimeStampPrev = mTimeStamp;
	// Setup shader:
	try {
		mGlslProg = gl::GlslProg::create(gl::GlslProg::Format()
			.vertex(loadAsset("kinect_depth.vert"))
			.fragment(loadAsset("kinect_depth.frag")));
	}
	catch (gl::GlslProgCompileExc ex) {
		console() << "GLSL Error: " << ex.what() << endl;
		quit();
	}
	catch (gl::GlslNullProgramExc ex) {
		console() << "GLSL Error: " << ex.what() << endl;
		quit();
	}
	catch (...) {
		console() << "Unknown GLSL Error" << endl;
		quit();
	}
	// Initialize Kinect and register callbacks:
	mDevice = Kinect2::Device::create();
	mDevice->start();
	mDevice->connectBodyIndexEventHandler([&](const Kinect2::BodyIndexFrame& frame)
	{
		mChannelBody = frame.getChannel();
	});
	mDevice->connectColorEventHandler([&](const Kinect2::ColorFrame& frame)
	{
		mSurfaceColor = frame.getSurface();
	});
	mDevice->connectDepthEventHandler([&](const Kinect2::DepthFrame& frame)
	{
		mChannelDepth = frame.getChannel();
		mTimeStamp = frame.getTimeStamp();
	});
}

void KinectDepthRangeApp::update()
{
	// Check whether depth-to-color mapping update is needed:
	if ((mTimeStamp != mTimeStampPrev) && mSurfaceColor && mChannelDepth) {
		// Update timestamp:
		mTimeStampPrev = mTimeStamp;
		// Initialize lookup surface:
		mSurfaceLookup = Surface32f(mChannelDepth.getWidth(), mChannelDepth.getHeight(), false, SurfaceChannelOrder::RGB);
		// Get depth-to-color mapping points:
		vector<ivec2> tMappingPoints = mDevice->mapDepthToColor(mChannelDepth);
		// Get color frame dimension:
		vec2 tColorFrameDim(Kinect2::ColorFrame().getSize());
		// Prepare iterators:
		Surface32f::Iter iter = mSurfaceLookup.getIter();
		vector<ivec2>::iterator v = tMappingPoints.begin();
		// Create lookup mapping:
		while (iter.line()) {
			while (iter.pixel()) {
				iter.r() = (float)v->x / tColorFrameDim.x;
				iter.g() = 1.0f - (float)v->y / tColorFrameDim.y;
				iter.b() = 0.0f;
				++v;
			}
		}
	}
}

void KinectDepthRangeApp::draw()
{
	// Prepare context:
	gl::viewport(getWindowSize());
	gl::clear();
	gl::setMatricesWindow(getWindowSize());
	gl::enableAlphaBlending();
	// Check for necessary inputs:
	if (mSurfaceColor && mChannelDepth && mSurfaceLookup && mChannelBody) {
		// Generate color texture:
		if (mTextureColor) {
			mTextureColor->update(mSurfaceColor);
		}
		else {
			mTextureColor = gl::Texture::create(mSurfaceColor);
		}
		// Bind color texture:
		mTextureColor->bind(0);
		// Generate depth texture:
		if (mTextureDepth) {
			mTextureDepth->update(Kinect2::channel16To8(mChannelDepth));
		}
		else {
			mTextureDepth = gl::Texture::create(Kinect2::channel16To8(mChannelDepth));
		}
		// Bind depth texture:
		mTextureDepth->bind(1);
		// Generate lookup texture:
		if (mTextureLookup) {
			mTextureLookup->update(mSurfaceLookup);
		}
		else {
			mTextureLookup = gl::Texture::create(mSurfaceLookup, gl::Texture::Format().dataType(GL_FLOAT));
		}
		// Bind lookup texture:
		mTextureLookup->bind(2);
		// Generate body-index texture:
		if (mTextureBody) {
			mTextureBody->update(Kinect2::colorizeBodyIndex(mChannelBody));
		}
		else {
			mTextureBody = gl::Texture::create(Kinect2::colorizeBodyIndex(mChannelBody));
		}
		// Bind body-index texture:
		mTextureBody->bind(3);
		// Bind shader and draw:
		{
			// Bind shader:
			gl::ScopedGlslProg shaderBind(mGlslProg);
			// Bind uniforms:
			gl::setDefaultShaderVars();
			mGlslProg->uniform("uTextureColor", 0);
			mGlslProg->uniform("uTextureDepth", 1);
			mGlslProg->uniform("uTextureLookup", 2);
			mGlslProg->uniform("uTextureBody", 3);
			// Set color:
			gl::color(1.0, 1.0, 1.0, 1.0);
			// Draw rect (TODO aspect ratio, etc):
			gl::drawSolidRect(getWindowBounds());
		}
		// Unbind textures:
		mTextureColor->unbind();
		mTextureDepth->unbind();
		mTextureLookup->unbind();
	}
}

CINDER_APP_BASIC(KinectDepthRangeApp, RendererGl)
