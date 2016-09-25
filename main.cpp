#include <osgViewer/Viewer>
#include <osg/GraphicsContext>
#include <osg/Camera>
#include <osg/Viewport>
#include <osg/StateSet>
#include <osg/Program>
#include <osg/Shader>

#include <iostream>
#include <thread>

float random(float min,float max) { return min + (max-min)*float(rand())/float(RAND_MAX); }

osg::Node* createScene(unsigned int nPoints);
void configureShaders( osg::StateSet* stateSet );

class SetupVAO
		: public osg::Camera::DrawCallback
{
public:
	SetupVAO();
	
	virtual ~SetupVAO() = default;
	
	virtual void operator () (osg::RenderInfo& renderInfo) const override;
	
private:
	mutable GLuint _vertexArrayObject;
};

double calc_pi( unsigned long n )
{
	double pi = 4.0 , decimal = 1.0;
	while( n > 2 ) {
		decimal -= ( 1.0 / ( 2.0 * n + 1 ) );
		--n;
		decimal += ( 1.0 / ( 2.0 * n + 1 ) );
		--n;
	}
	if( n > 0 )
		decimal -= ( 1.0 / ( 2.0 * n + 1 ) );
	return pi * decimal;
}

int main( int argc, char** argv )
{
	osg::ArgumentParser arguments( &argc, argv );

	const int width( 800 ), height( 450 );
	const std::string version( "3.3" );
	osg::ref_ptr< osg::GraphicsContext::Traits > traits = new osg::GraphicsContext::Traits();
	traits->x = 20; traits->y = 30;
	traits->width = width; traits->height = height;
	traits->windowDecoration = true;
	traits->doubleBuffer = true;
	traits->glContextVersion = version;
	osg::ref_ptr< osg::GraphicsContext > gc = osg::GraphicsContext::createGraphicsContext( traits.get() );
	
	if( !gc.valid() ) {
		osg::notify( osg::FATAL ) << "Unable to create OpenGL v" << version << " context." << std::endl;
		return 1;
	}

	osgViewer::Viewer viewer(arguments);

	// Create a Camera that uses the above OpenGL context.
	auto* cam = viewer.getCamera();
	cam->setGraphicsContext( gc.get() );
	// Must set perspective projection for fovy and aspect.
	cam->setProjectionMatrix( osg::Matrix::perspective( 30., double(width)/double(height), 1., 100. ) );
	// Unlike OpenGL, OSG viewport does *not* default to window dimensions.
	cam->setViewport( new osg::Viewport( 0, 0, width, height ) );
	cam->setInitialDrawCallback(new SetupVAO());

	osg::ref_ptr<osg::Node> root = createScene(500);

	configureShaders( root->getOrCreateStateSet() );

	viewer.setSceneData( root );
	viewer.setRunFrameScheme(osgViewer::ViewerBase::ON_DEMAND);
	viewer.setThreadingModel(osgViewer::ViewerBase::SingleThreaded);

	// for non GL3/GL4 and non GLES2 platforms we need enable the osg_ uniforms that the shaders will use,
	// you don't need thse two lines on GL3/GL4 and GLES2 specific builds as these will be enable by default.
	auto state = gc->getState();

	// Required for core profile, as VAO 0 is inmutable
	state->setUseModelViewAndProjectionUniforms(true);
	state->setUseVertexAttributeAliasing(false);
	state->setCheckForGLErrors(osg::State::ONCE_PER_ATTRIBUTE);
	
	viewer.realize();
	
	std::cout << "Start" << std::endl;
	std::vector<std::thread> workers;
	for (unsigned int i = 0; i < 4; ++i) {
		double value;
		workers.emplace_back([&value]() { value = calc_pi(1000000000ull); });
	}

	std::for_each(workers.begin(), workers.end(), [](std::thread &t) { t.join(); });
	std::cout << "Done" << std::endl;
	return viewer.run();
}

osg::Node* createScene(unsigned int nPoints)
{
	auto geometry = new osg::Geometry;
	geometry->setUseDisplayList(false);
	geometry->setUseVertexBufferObjects(true);

	typedef osg::UShortArray ValueArray;
	auto vertices = new osg::Vec3Array(nPoints);
	auto intensity = new ValueArray(nPoints);
	constexpr float min = -1.0f;
	constexpr float max = 1.0f;
	for (auto i = 0u; i < nPoints; ++i) {
		vertices->at(i).set(random(min, max), random(min, max), random(min, max));
		intensity->at(i) = ValueArray::value_type(random(0.0, float(std::numeric_limits<ValueArray::value_type>::max())));
	}

	geometry->setVertexAttribArray(0, vertices, osg::Array::BIND_PER_VERTEX);
	geometry->setVertexAttribArray(2, intensity, osg::Array::BIND_PER_VERTEX);
	geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, GLsizei(nPoints)));

	auto geode = new osg::Geode;
	geode->addDrawable(geometry);

	auto group = new osg::Group;
	group->addChild(geode);

	return group;
}


SetupVAO::SetupVAO()
	: _vertexArrayObject(0)
{}

void SetupVAO::operator () (osg::RenderInfo& renderInfo) const
{
	auto extensions = renderInfo.getState()->get<osg::GLExtensions>();
	if (_vertexArrayObject == 0) {
		extensions->glGenVertexArrays(1, &_vertexArrayObject);
	}
	extensions->glBindVertexArray(_vertexArrayObject);
}



void configureShaders( osg::StateSet* stateSet )
{
	const std::string vertexSource =
		"#version 330 core\n"
		"\n"
		"uniform mat4 osg_ModelViewProjectionMatrix;\n"
		"\n"
		"layout(location=0) in vec4 osg_Vertex;\n"
		"layout(location=2) in uint scale;\n"
		"out vec4 vColor;\n"
		"\n"
		"void main()\n"
		"{\n"
		"	vColor = vec4( vec3(float(scale) * (1.0/65535.0)), 1.);\n"
		"	gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;\n"
		"}\n";
	osg::Shader* vShader = new osg::Shader( osg::Shader::VERTEX, vertexSource );

	const std::string fragmentSource =
		"#version 330\n"
		"\n"
		"in vec4 vColor;\n"
		"out vec4 fragData;\n"
		"\n"
		"void main()\n"
		"{\n"
		"	fragData = vColor;\n"
		"}\n";
	osg::Shader* fShader = new osg::Shader( osg::Shader::FRAGMENT, fragmentSource );

	osg::Program* program = new osg::Program;
	program->addShader( vShader );
	program->addShader( fShader );
	stateSet->setAttribute( program );
}
