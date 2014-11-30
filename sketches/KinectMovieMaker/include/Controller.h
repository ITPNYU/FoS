/* ITP Future of Storytelling */

#pragma once

#include "Kinect2.h"

#include "cinder/Utilities.h"
#include "cinder/qtime/QuickTimeGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/app/AppBasic.h"
#include "cinder/app/RendererGl.h"

#include "State.h"
#include "Directive.h"

class Controller : public State {
public:

	typedef std::shared_ptr<Controller>			Ref;
	typedef std::shared_ptr<const Controller>	ConstRef;

private:

	ci::app::AppBasic*			mAppPtr;

	Directive::RefDeque			mDirectiveStack;
	Directive::RefMap			mDirectiveMap;

	std::size_t					mBodyCount;

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

	ci::qtime::MovieGlRef		mMovieBackground;
	ci::qtime::MovieGlRef		mMovieForeground;

	ci::gl::GlslProgRef			mGlslProg;

	/** @brief private constructor */
	Controller(ci::app::AppBasic* iAppPtr, const std::string& iName) : 
		State(iName),
		mAppPtr(iAppPtr), 
		mBodyCount(0)
	{ /* no-op */ }

	/** @brief initialization method */
	void initialize()
	{
		// Enable texture mode:
		ci::gl::enable(GL_TEXTURE_2D);
		// Initialize timestamp:
		mTimeStamp = 0L;
		mTimeStampPrev = mTimeStamp;
		// Setup shader:
		try {
			mGlslProg = ci::gl::GlslProg::create(ci::gl::GlslProg::Format()
				.vertex(ci::app::loadAsset("kinect_depth.vert"))
				.fragment(ci::app::loadAsset("kinect_depth.frag")));
		}
		catch (ci::gl::GlslProgCompileExc ex) {
			ci::app::console() << "GLSL Error: " << ex.what() << std::endl;
			mAppPtr->quit();
		}
		catch (ci::gl::GlslNullProgramExc ex) {
			ci::app::console() << "GLSL Error: " << ex.what() << std::endl;
			mAppPtr->quit();
		}
		catch (...) {
			ci::app::console() << "Unknown GLSL Error" << std::endl;
			mAppPtr->quit();
		}
		// Initialize Kinect and register callbacks:
		mDevice = Kinect2::Device::create();
		mDevice->start();
		mDevice->connectBodyEventHandler([&](const Kinect2::BodyFrame& frame)
		{
			// Get active bodies:
			const std::vector<Kinect2::Body>& tBodies = frame.getBodies();
			std::vector<const Kinect2::Body*> tActiveBodies;
			for (std::vector<Kinect2::Body>::const_iterator it = tBodies.cbegin(); it != tBodies.cend(); it++) {
				if ((*it).calcConfidence() > 0.5) { 
					tActiveBodies.push_back(&(*it));
				}
			}
			// Get active count:
			size_t tActiveCount = tActiveBodies.size();
			// Set body count change:
			if (mBodyCount != tActiveCount) {
				if (tActiveCount == 0) {
					// Remove conflicting directives, if applicable:
					popDirective("too_many_users");
					// Push directive:
					pushDirective("no_user");
				}
				else if (tActiveCount > 1) {
					// Remove conflicting directives, if applicable:
					popDirective("no_user");
					// Push directive:
					pushDirective("too_many_users");
				}
				else {
					// Remove conflicting directives, if applicable:
					popDirective("no_user");
					popDirective("too_many_users");
				}
				// Set new count:
				mBodyCount = tActiveCount;
			}
			// 
			else if (mBodyCount == 1) {
				// TODO...?
			}
		});
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
		// Add directives:
		addDirective("no_background_video", "Please drag a background video onto this app!", 10);
		addDirective("no_foreground_video", "Please drag a foreground video onto this app!", 10);
		addDirective("no_user", "Is anyone there?", 1);
		addDirective("too_many_users", "One person at a time, please!", 1);
		// Load initial directives:
		pushDirective("no_user");
		pushDirective("no_background_video");
		// TODO: Reinstate?
		pushDirective("no_foreground_video");
	}

public:

	/** @brief destructor */
	~Controller() { /* no-op */ }

	/** @brief static creational method */
	template <typename ... T> static Controller::Ref create(T&& ... args)
	{
		Controller::Ref t = Controller::Ref(new Controller(std::forward<T>(args)...));
		t->initialize();
		return t;
	}

	/** @brief returns a const shared_ptr to this controller object */
	Controller::ConstRef getRef() const { return std::dynamic_pointer_cast<const Controller>(getBaseRef()); }

	/** @brief returns a shared_ptr to this controller object */
	Controller::Ref getRef() { return std::dynamic_pointer_cast<Controller>(getBaseRef()); }

	/** @brief update method */
	void update()
	{
		// Check whether depth-to-color mapping update is needed:
		if ((mTimeStamp != mTimeStampPrev) && mSurfaceColor && mChannelDepth) {
			// Update timestamp:
			mTimeStampPrev = mTimeStamp;
			// Initialize lookup surface:
			mSurfaceLookup = ci::Surface32f(mChannelDepth.getWidth(), mChannelDepth.getHeight(), false, ci::SurfaceChannelOrder::RGB);
			// Get depth-to-color mapping points:
			std::vector<ci::ivec2> tMappingPoints = mDevice->mapDepthToColor(mChannelDepth);
			// Get color frame dimension:
			ci::vec2 tColorFrameDim(Kinect2::ColorFrame().getSize());
			// Prepare iterators:
			ci::Surface32f::Iter iter = mSurfaceLookup.getIter();
			std::vector<ci::ivec2>::iterator v = tMappingPoints.begin();
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

	/** @brief draw method */
	void draw()
	{
		// Handle background movie, if available:
		if (mMovieBackground) {
			ci::gl::TextureRef tMovieTexture = mMovieBackground->getTexture();
			if (tMovieTexture) {
				// Set color:
				ci::gl::color(1.0, 1.0, 1.0, 1.0);
				// Draw texture:
				ci::gl::draw(tMovieTexture, ci::vec2(0.0f, 0.0f));
			}
		}
		// Check for necessary inputs:
		if (mSurfaceColor && mChannelDepth && mSurfaceLookup && mChannelBody) {
			// Generate color texture:
			if (mTextureColor) {
				mTextureColor->update(mSurfaceColor);
			}
			else {
				mTextureColor = ci::gl::Texture::create(mSurfaceColor);
			}
			// Bind color texture:
			mTextureColor->bind(0);
			// Generate depth texture:
			if (mTextureDepth) {
				mTextureDepth->update(Kinect2::channel16To8(mChannelDepth));
			}
			else {
				mTextureDepth = ci::gl::Texture::create(Kinect2::channel16To8(mChannelDepth));
			}
			// Bind depth texture:
			mTextureDepth->bind(1);
			// Generate lookup texture:
			if (mTextureLookup) {
				mTextureLookup->update(mSurfaceLookup);
			}
			else {
				mTextureLookup = ci::gl::Texture::create(mSurfaceLookup, ci::gl::Texture::Format().dataType(GL_FLOAT));
			}
			// Bind lookup texture:
			mTextureLookup->bind(2);
			// Generate body-index texture:
			if (mTextureBody) {
				mTextureBody->update(mChannelBody);
				//mTextureBody->update(Kinect2::colorizeBodyIndex(mChannelBody));
			}
			else {
				mTextureBody = ci::gl::Texture::create(mChannelBody);
				//mTextureBody = ci::gl::Texture::create(Kinect2::colorizeBodyIndex(mChannelBody));
			}
			// Bind body-index texture:
			mTextureBody->bind(3);
			// Bind shader and draw:
			{
				// Bind shader:
				ci::gl::ScopedGlslProg shaderBind(mGlslProg);
				// Bind uniforms:
				ci::gl::setDefaultShaderVars();
				mGlslProg->uniform("uTextureColor", 0);
				mGlslProg->uniform("uTextureDepth", 1);
				mGlslProg->uniform("uTextureLookup", 2);
				mGlslProg->uniform("uTextureBody", 3);
				// Set color:
				ci::gl::color(1.0, 1.0, 1.0, 1.0);
				// Draw rect (TODO aspect ratio, etc):
				//Rectf tColorBounds = mTextureColor->getBounds();
				//gl::drawSolidRect(tColorBounds.getCenteredFit(getWindowBounds(), false));
				ci::gl::drawSolidRect(ci::app::getWindowBounds());
			}
			// Unbind textures:
			mTextureColor->unbind();
			mTextureDepth->unbind();
			mTextureLookup->unbind();
		}
		// Handle foreground movie, if available:
		if (mMovieForeground) {
			ci::gl::TextureRef tMovieTexture = mMovieForeground->getTexture();
			if (tMovieTexture) {
				// Set color:
				ci::gl::color(1.0, 1.0, 1.0, 1.0);
				// Draw texture:
				ci::gl::draw(tMovieTexture, ci::vec2(0.0f, 0.0f));
			}
		}
		// Draw active directive, if available:
		if (!mDirectiveStack.empty()) {
			mDirectiveStack.back()->draw();
		}
	}

	/** @brief file-drop handler method */
	void fileDrop(ci::app::FileDropEvent event)
	{
		if (event.getNumFiles() == 1) {
			const ci::fs::path& tPath = event.getFile(0);
			// Handle background:
			if (isTopDirective("no_background_video")) {
				try { 
					mMovieBackground = ci::qtime::MovieGl::create(tPath);
					mMovieBackground->setLoop(true, false); // TODO
					mMovieBackground->play(); // TODO
				}
				catch (ci::Exception &exc) { return; }
				popDirective("no_background_video");
			}
			// Handle foreground:
			else if (isTopDirective("no_foreground_video")) {
				try { 
					mMovieForeground = ci::qtime::MovieGl::create(tPath);
					mMovieForeground->setLoop(true, false); // TODO
					mMovieForeground->play(); // TODO
				}
				catch (ci::Exception &exc) { return; }
				popDirective("no_foreground_video");
			}
			else {
				// TODO: Handle other cases?
			}
		}
		else {
			// TODO: Push incorrect usage directive?
		}
	}

	/** @brief adds a new directive to map */
	void addDirective(const std::string& iName, const std::string& iLabel, const int& iPriority)
	{
		// Create directive:
		Directive::Ref tDirective = Directive::create(iName, iLabel, iPriority);
		// Add directive to map:
		mDirectiveMap[iName] = tDirective;
	}

	void pushDirective(const std::string& iName)
	{
		Directive::RefMap::iterator itFind = mDirectiveMap.find(iName);
		if (itFind != mDirectiveMap.end()) {
			// If directive is already in stack, remove it:
			popDirective((*itFind).second);
			// Find insertion position:
			Directive::RefDeque::iterator it = mDirectiveStack.begin();
			while (it != mDirectiveStack.end() && (*it)->getPriority() < (*itFind).second->getPriority()) {
				// Advance iter:
				it++;
			}
			// Insert at iterator:
			mDirectiveStack.insert(it, (*itFind).second);
		}
	}

	void popDirective(Directive::Ref iDirective)
	{
		// If directive is in stack, remove it:
		for (Directive::RefDeque::iterator it = mDirectiveStack.begin(); it != mDirectiveStack.end(); it++) {
			if ((*it).get() == iDirective.get()) {
				mDirectiveStack.erase(it);
				return;
			}
		}
	}

	void popDirective(const std::string& iName)
	{
		// If directive is in stack, remove it:
		for (Directive::RefDeque::iterator it = mDirectiveStack.begin(); it != mDirectiveStack.end(); it++) {
			if ((*it)->getName() == iName) {
				mDirectiveStack.erase(it);
				return;
			}
		}
	}

	bool isTopDirective(Directive::Ref iDirective) const
	{
		if (mDirectiveStack.empty()) { return false; }
		return (mDirectiveStack.back().get() == iDirective.get());
	}

	bool isTopDirective(const std::string& iName) const
	{
		if (mDirectiveStack.empty()) { return false; }
		return (mDirectiveStack.back()->getName() == iName);
	}
};
