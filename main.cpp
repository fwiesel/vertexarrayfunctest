#include <osgViewer/Viewer>
#include <osg/GraphicsContext>
#include <osg/Camera>
#include <osg/Viewport>
#include <osg/StateSet>
#include <osg/Program>
#include <osg/Shader>

float random(float min,float max) { return min + (max-min)*float(rand())/float(RAND_MAX); }

/* Required to work on mac os x in the core profile, as it does not have the default 0 VAO */
struct VAODrawCallBack;
osg::Node* createScene(unsigned int nPoints);
void configureShaders( osg::StateSet* stateSet );

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
    cam->setProjectionMatrix( osg::Matrix::perspective( 30., (double)width/(double)height, 1., 100. ) );
    // Unlike OpenGL, OSG viewport does *not* default to window dimensions.
    cam->setViewport( new osg::Viewport( 0, 0, width, height ) );

	osg::ref_ptr<osg::Node> root = createScene(500);

    configureShaders( root->getOrCreateStateSet() );
	
    viewer.setSceneData( root );
    viewer.setRunFrameScheme(osgViewer::ViewerBase::ON_DEMAND);

    // for non GL3/GL4 and non GLES2 platforms we need enable the osg_ uniforms that the shaders will use,
    // you don't need thse two lines on GL3/GL4 and GLES2 specific builds as these will be enable by default.
    auto state = gc->getState();
	
    state->setUseModelViewAndProjectionUniforms(true);
    state->setUseVertexAttributeAliasing(false);
    state->setCheckForGLErrors(osg::State::ONCE_PER_ATTRIBUTE);

    return viewer.run();
}

struct VAODrawCallBack : public osg::Drawable::DrawCallback
{
	VAODrawCallBack()
		: osg::Drawable::DrawCallback()
		, _vertexArrayObject(0)
	{}

	VAODrawCallBack(const osg::Drawable::DrawCallback& org, const osg::CopyOp& copyop)
		: DrawCallback(org, copyop)
		, _vertexArrayObject(0)
	{}

	META_Object(osg,VAODrawCallBack)

	virtual void drawImplementation(osg::RenderInfo& renderInfo, const osg::Drawable* drawable) const;
	mutable GLuint _vertexArrayObject;
};

void VAODrawCallBack::drawImplementation(osg::RenderInfo& renderInfo, const osg::Drawable* drawable) const {
	auto state = renderInfo.getState();
	osg::notify( osg::INFO ) << "State: " << (state->getUseVertexAttributeAliasing() ? "true" : "false") << std::endl;
	auto extensions = state->get<osg::GLExtensions>();
	if (0 == _vertexArrayObject) {
		extensions->glGenVertexArrays(1, &_vertexArrayObject);
	}
	extensions->glBindVertexArray(_vertexArrayObject);
	drawable->drawImplementation(renderInfo);
}

osg::Node* createScene(unsigned int nPoints)
{
	auto geometry = new osg::Geometry;
	geometry->setUseDisplayList(false);
	geometry->setUseVertexBufferObjects(true);

	auto vertices = new osg::Vec3Array(nPoints);
	constexpr float min = -1.0f;
	constexpr float max = 1.0f;
	for (auto i = 0u; i < nPoints; ++i) {
		vertices->at(i).set(random(min, max), random(min, max), random(min, max));
	}

	geometry->setVertexAttribArray(0, vertices, osg::Array::BIND_PER_VERTEX);
	geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, GLsizei(nPoints)));
	geometry->setDrawCallback(new VAODrawCallBack());
	
	auto geode = new osg::Geode;
	geode->addDrawable(geometry);

	auto group = new osg::Group;
	group->addChild(geode);

	return group;
}


void configureShaders( osg::StateSet* stateSet )
{
    const std::string vertexSource =
        "#version 330 core\n"
        "\n"
        "uniform mat4 osg_ModelViewProjectionMatrix;\n"
        "\n"
        "layout(location=0) in vec4 osg_Vertex;\n"
        "out vec4 color;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    color = vec4( 1., 1., 1., 1.);\n"
        "    gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;\n"
        "}\n";
    osg::Shader* vShader = new osg::Shader( osg::Shader::VERTEX, vertexSource );

    const std::string fragmentSource =
        "#version 330\n"
        "\n"
        "in vec4 color;\n"
        "out vec4 fragData;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    fragData = color;\n"
        "}\n";
    osg::Shader* fShader = new osg::Shader( osg::Shader::FRAGMENT, fragmentSource );

    osg::Program* program = new osg::Program;
    program->addShader( vShader );
    program->addShader( fShader );
    stateSet->setAttribute( program );
}
