#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/params/Params.h"

#include "Kinect2.h"

using namespace ci;
using namespace ci::app;
using namespace std;

/** @brief abstract base class for all entity types */
class Entity : public std::enable_shared_from_this<Entity> {
public:

	typedef std::shared_ptr<Entity>			Ref;
	typedef std::shared_ptr<const Entity>	ConstRef;
	typedef std::weak_ptr<Entity>			WeakRef;

protected:

	/** @brief protected default constructor */
	Entity() { /* no-op */ }

	/** @brief overloadable initialization method */
	virtual void initialize() = 0;

public:

	/** @brief virtual destructor */
	virtual ~Entity() { /* no-op */ }

	/** @brief overloadable update method */
	virtual void update() = 0;

	/** @brief overloadable draw method */
	virtual void draw() = 0;

	/** @brief returns a const shared_ptr to this entity */
	Entity::ConstRef getEntityRef() const { return shared_from_this(); }

	/** @brief returns a shared_ptr to this entity */
	Entity::Ref getEntityRef() { return shared_from_this(); }
};

/** @brief controller implementation */
class Controller : public Entity {
public:

	typedef std::shared_ptr<Controller>			Ref;
	typedef std::shared_ptr<const Controller>	ConstRef;
	typedef std::weak_ptr<Controller>			WeakRef;

private:

	/** @brief private default constructor */
	Controller() : Entity() { /* no-op */ }

public:

	/** @brief destructor */
	~Controller() { /* no-op */ }

	/** @brief static creational method */
	template<typename ... T> static Controller::Ref create(T&& ... args)
	{
		Controller::Ref t = Controller::Ref(new Controller(std::forward<T>(args) ...));
		t->initialize();
		return t;
	}

	/** @brief returns a const shared_ptr to this controller */
	Controller::ConstRef getRef() const { return std::dynamic_pointer_cast<const Controller>( getEntityRef() ); }

	/** @brief returns a shared_ptr to this controller */
	Controller::Ref getRef() { return std::dynamic_pointer_cast<Controller>( getEntityRef() ); }
};

/** @brief abstract base class for directives */
class Directive {
public:

	// TODO: This should be derived from Entity.

	typedef std::shared_ptr<Directive> Ref;

protected:

	std::string		mLabel;			//!< the directive label
	Controller::Ref mController;	//!< the controller

	/** @brief protected default constructor */
	Directive() { /* no-op */ }

	/** @brief overloadable initialization method */
	virtual void initialize() = 0;

public:

	/** @brief virtual destructor */
	virtual ~Directive() { /* no-op */ }

	/** @brief returns a const reference to the directive label */
	const std::string& getLabel() const { return mLabel; }

	/** @brief sets the directive label from input */
	void setLabel(const std::string& iLabel) { mLabel = iLabel; }

	/** @brief returns a const shared_ptr to controller */
	Controller::ConstRef getController() const { return mController; }

	/** @brief returns a shared_ptr to controller */
	Controller::Ref getController() { return mController; }

	/** @brief overloadable update method */
	virtual void update() = 0;

	/** @brief overloadable draw method */
	virtual void draw() = 0;
};


class KinectDepthRangeApp : public AppNative {
public:

	void prepareSettings(ci::app::AppBasic::Settings* settings);
	
	void setup();
	void update();
	void draw();

private:

	Kinect2::BodyFrame			mBodyFrame;
	ci::Channel8u				mChannelBodyIndex;
	ci::Channel16u				mChannelDepth;
	Kinect2::DeviceRef			mDevice;

	float						mFrameRate;
	bool						mFullScreen;
	ci::params::InterfaceGlRef	mParams;
};

void KinectDepthRangeApp::prepareSettings(Settings* settings)
{
	settings->prepareWindow( Window::Format().size(1024, 768).title( "ITP FoS Sketch" ) );
	settings->setFrameRate( 60.0f );
}

void KinectDepthRangeApp::setup()
{
	mFrameRate = 0.0f;
	mFullScreen = false;

	mDevice = Kinect2::Device::create();
	mDevice->start();
	mDevice->connectBodyEventHandler([&](const Kinect2::BodyFrame frame)
	{
		mBodyFrame = frame;
	});
	mDevice->connectBodyIndexEventHandler([&](const Kinect2::BodyIndexFrame frame)
	{
		mChannelBodyIndex = frame.getChannel();
	});
	mDevice->connectDepthEventHandler([&](const Kinect2::DepthFrame frame)
	{
		mChannelDepth = frame.getChannel();
	});

	mParams = params::InterfaceGl::create("Params", Vec2i(200, 100));
	mParams->addParam("Frame rate", &mFrameRate, "", true);
	mParams->addParam("Full screen", &mFullScreen).key("f");
	mParams->addButton("Quit", [&]() { quit(); }, "key=q");
}

void KinectDepthRangeApp::update()
{
	mFrameRate = getAverageFps();

	if (mFullScreen != isFullScreen()) {
		setFullScreen(mFullScreen);
		mFullScreen = isFullScreen();
	}
}

void KinectDepthRangeApp::draw()
{
	gl::setViewport(getWindowBounds());
	gl::clear(Colorf::black());
	gl::color(ColorAf::white());
	gl::disableDepthRead();
	gl::disableDepthWrite();
	gl::enableAlphaBlending();

	// DEPTH DATA:

	if (mChannelDepth) {
		gl::enable(GL_TEXTURE_2D);
		gl::TextureRef tex = gl::Texture::create(Kinect2::channel16To8(mChannelDepth));
		gl::draw(tex, tex->getBounds(), Rectf(getWindowBounds()));
	}

	// USER MASK:

	if (mChannelBodyIndex) {
		gl::enable(GL_TEXTURE_2D);
		gl::color(ColorAf(Colorf::white(), 0.15f));
		gl::TextureRef tex = gl::Texture::create(Kinect2::colorizeBodyIndex(mChannelBodyIndex));
		gl::draw(tex, tex->getBounds(), Rectf(getWindowBounds()));
	}

	mParams->draw();
}

CINDER_APP_NATIVE( KinectDepthRangeApp, RendererGl )
