#include "pch.h"
#include "nuRenderGL.h"
#include "../Image/nuImage.h"

#define Z(s) #s

static const char* pFillVert = Z(
	uniform		mat4	mvproj;
	attribute	vec4	vpos;
	attribute	vec4	vcolor;
	varying		vec4	color;
	void main()
	{
		gl_Position = mvproj * vpos;
		color = vcolor;
	}
);

static const char* pFillFrag = Z(
	precision mediump float;
	varying vec4	color;
	void main()
	{
		gl_FragColor = color;
	}
);

static const char* pFillTexVert = Z(
	uniform		mat4	mvproj;
	attribute	vec4	vpos;
	attribute	vec4	vcolor;
	attribute	vec2	vtexuv0;
	varying		vec4	color;
	varying		vec2	texuv0;
	void main()
	{
		gl_Position = mvproj * vpos;
		texuv0 = vtexuv0;
		color = vcolor;
	}
);

static const char* pFillTexFrag = Z(
	precision mediump float;
	uniform sampler2D	tex0;
	varying vec4		color;
	varying vec2		texuv0;
	void main()
	{
		gl_FragColor = color * texture2D(tex0, texuv0.st);
	}
);

static const char* pRectVert = Z(
	uniform		mat4	mvproj;
	attribute	vec4	vpos;
	attribute	vec4	vcolor;
	varying		vec4	pos;
	varying		vec4	color;
	void main()
	{
		pos = mvproj * vpos;
		//pos = mvproj * gl_Vertex;
		//pos = gl_ModelViewProjectionMatrix * gl_Vertex;
		//pos = vpos;
		gl_Position = pos;
		//gl_FrontColor = vcolor;
		color = vcolor;
	}
);

static const char* pRectFrag = Z(
	precision mediump float;
	varying vec4	pos;
	varying vec4	color;
	uniform float	radius;
	uniform vec4	box;
	uniform vec2	vport_hsize;

	vec2 to_screen( vec2 unit_pt )
	{
		return (vec2(unit_pt.x, -unit_pt.y) + vec2(1,1)) * vport_hsize;
	}

	void main()
	{
		vec2 screenxy = to_screen(pos.xy);
		float left = box.x + radius;
		float right = box.z - radius;
		float top = box.y + radius;
		float bottom = box.w - radius;
		vec2 cent = screenxy;
		float iradius = radius;
		if (		screenxy.x < left && screenxy.y < top )		{ cent = vec2(left, top); }
		else if (	screenxy.x < left && screenxy.y > bottom )	{ cent = vec2(left, bottom); }
		else if (	screenxy.x > right && screenxy.y < top )	{ cent = vec2(right, top); }
		else if (	screenxy.x > right && screenxy.y > bottom )	{ cent = vec2(right, bottom); }
		else iradius = 10.0;

		float dist = length( screenxy - cent );

		gl_FragColor = color;
		gl_FragColor.a *= clamp(iradius - dist, 0.0, 1.0);
		//gl_FragColor.r += 0.3;
		//gl_FragColor.a += 0.3;
	}
);

static const char* pCurveVert = Z(
	varying vec4 pos;
	varying vec2 texuv0;
	void main()
	{
		gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
		gl_FrontColor = gl_Color;
		pos = gl_Position;
		texuv0 = gl_MultiTexCoord0.xy;
	}
);

// This is from Jim Blinn and Charles Loop's paper "Resolution Independent Curve Rendering using Programmable Graphics Hardware"
// We don't need this complexity here.. and if I recall correctly, this technique aliases under minification faster than
// our simpler rounded-rectangle alternative.
static const char* pCurveFrag = Z(
	varying vec4 pos;
	varying vec2 texuv0;

	void main()
	{
		vec2 p = texuv0;

		// Gradients
		vec2 px = dFdx(p);
		vec2 py = dFdy(p);

		// Chain rule
		float fx = (2 * p.x) * px.x - px.y;
		float fy = (2 * p.x) * py.x - py.y;

		// Signed distance
		float sd = (p.x * p.x - p.y) / sqrt(fx * fx + fy * fy);

		// Linear alpha
		float alpha = 0.5 - sd;

		gl_FragColor = gl_Color;

		if ( alpha > 1 )
			gl_FragColor.a = 1;
		else if ( alpha < 0 )
			discard;
		else
			gl_FragColor.a = alpha;
	}
);

nuGLProg::nuGLProg()
{
	memset(this, 0, sizeof(*this));
}

nuRenderGL::nuRenderGL()
{
	Reset();
}

nuRenderGL::~nuRenderGL()
{
}

void nuRenderGL::Reset()
{
	memset( this, 0, sizeof(*this) );
}

bool nuRenderGL::CreateShaders()
{
	Check();

	if ( SingleTex2D == 0 )
		glGenTextures( 1, &SingleTex2D );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, SingleTex2D );

	Check();

	if ( !LoadProgram( PRect, pRectVert, pRectFrag ) ) return false;
	if ( !LoadProgram( PFill, pFillVert, pFillFrag ) ) return false;
	if ( !LoadProgram( PFillTex, pFillTexVert, pFillTexFrag ) ) return false;
	//if ( !LoadProgram( PCurve, pCurveVert, pCurveFrag ) ) return false;

	//glUseProgram( PRect.Prog );
	VarRectBox = glGetUniformLocation( PRect.Prog, "box" );
	VarRectRadius = glGetUniformLocation( PRect.Prog, "radius" );
	VarRectVPortHSize = glGetUniformLocation( PRect.Prog, "vport_hsize" );
	VarRectMVProj = glGetUniformLocation( PRect.Prog, "mvproj" );
	VarRectVPos = glGetAttribLocation( PRect.Prog, "vpos" );
	VarRectVColor = glGetAttribLocation( PRect.Prog, "vcolor" );
	//VarCurveTex0 = glGetAttribLocation( PCurve.Prog, "vtex0" );

	//glUseProgram( PFill.Prog );
	VarFillMVProj = glGetUniformLocation( PFill.Prog, "mvproj" );
	VarFillVPos = glGetAttribLocation( PFill.Prog, "vpos" );
	VarFillVColor = glGetAttribLocation( PFill.Prog, "vcolor" );

	//glUseProgram( PFillTex.Prog );
	VarFillTexMVProj = glGetUniformLocation( PFillTex.Prog, "mvproj" );
	VarFillTexVPos = glGetAttribLocation( PFillTex.Prog, "vpos" );
	VarFillTexVColor = glGetAttribLocation( PFillTex.Prog, "vcolor" );
	VarFillTexVUV = glGetAttribLocation( PFillTex.Prog, "vtexuv0" );
	VarFillTex0 = glGetUniformLocation( PFillTex.Prog, "tex0" );

	NUTRACE( "VarFillMVProj = %d\n", VarFillMVProj );
	NUTRACE( "VarFillVPos = %d\n", VarFillVPos );
	NUTRACE( "VarFillVColor = %d\n", VarFillVColor );

	NUTRACE( "VarFillTexMVProj = %d\n", VarFillTexMVProj );
	NUTRACE( "VarFillTexVPos = %d\n", VarFillTexVPos );
	NUTRACE( "VarFillTexVColor = %d\n", VarFillTexVColor );
	NUTRACE( "VarFillTexVUV = %d\n", VarFillTexVUV );
	NUTRACE( "VarFillTex0 = %d\n", VarFillTex0 );

	//glUseProgram( 0 );
	
	Check();

	return true;
}

void nuRenderGL::DeleteShaders()
{
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	if ( SingleTex2D != 0 )
		glDeleteTextures( 1, &SingleTex2D );
	SingleTex2D = 0;

	glUseProgram( 0 );
	DeleteProgram( PFill );
	DeleteProgram( PFillTex );
	DeleteProgram( PRect );
	DeleteProgram( PCurve );
}

void nuRenderGL::SurfaceLost()
{
	Reset();
	CreateShaders();
}

void nuRenderGL::ActivateProgram( nuGLProg& p )
{
	if ( ActiveProgram == &p ) return;
	ActiveProgram = &p;
	NUASSERT( p.Prog != 0 );
	glUseProgram( p.Prog );
	Check();
}

void nuRenderGL::Ortho( Mat4f &imat, double left, double right, double bottom, double top, double znear, double zfar )
{
	Mat4f m;
	m.Zero();
	double A = 2 / (right - left);
	double B = 2 / (top - bottom);
	double C = -2 / (zfar - znear);
	double tx = -(right + left) / (right - left);
	double ty = -(top + bottom) / (top - bottom);
	double tz = -(zfar + znear) / (zfar - znear);
	m.m(0,0) = (float) A;
	m.m(1,1) = (float) B;
	m.m(2,2) = (float) C;
	m.m(3,3) = 1;
	m.m(0,3) = (float) tx;
	m.m(1,3) = (float) ty;
	m.m(2,3) = (float) tz;
	imat = imat * m;
}

void nuRenderGL::PreRender( int fbwidth, int fbheight )
{
	Check();

	NUTRACE_RENDER( "PreRender %d %d\n", fbwidth, fbheight );
	Check();

	FBWidth = fbwidth;
	FBHeight = fbheight;
	glViewport( 0, 0, fbwidth, fbheight );

	//glMatrixMode( GL_PROJECTION );
	//glLoadIdentity();
	//glOrtho( 0, fbwidth, fbheight, 0, 0, 1 );

	//glMatrixMode( GL_MODELVIEW );
	//glLoadIdentity();

	// Make our clear color a very noticeable purple, so you know when you've screwed up the root node
	glClearColor( 0.7f, 0.0, 0.7f, 0 );

	//glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_ACCUM_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

	NUTRACE_RENDER( "PreRender 2\n" );
	Check();

	// Enable CULL_FACE because it will make sure that we are consistent about vertex orientation
	//glEnable( GL_CULL_FACE );
	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	NUTRACE_RENDER( "PreRender 3\n" );
	Check();

	Mat4f mvproj;
	mvproj.Identity();
	Ortho( mvproj, 0, fbwidth, fbheight, 0, 0, 1 );
	// GLES doesn't support TRANSPOSE = TRUE
	mvproj = mvproj.Transposed();

	ActivateProgram( PRect );
	glUniform2f( VarRectVPortHSize, FBWidth / 2.0f, FBHeight / 2.0f );
	glUniformMatrix4fv( VarRectMVProj, 1, false, &mvproj.row[0].x );
	Check();

	ActivateProgram( PFill );

	NUTRACE_RENDER( "PreRender 4 (%d)\n", VarFillMVProj );
	Check();

	glUniformMatrix4fv( VarFillMVProj, 1, false, &mvproj.row[0].x );

	NUTRACE_RENDER( "PreRender 5\n" );
	Check();

	ActivateProgram( PFillTex );

	NUTRACE_RENDER( "PreRender 6 (%d)\n", VarFillTexMVProj );
	Check();

	glUniformMatrix4fv( VarFillTexMVProj, 1, false, &mvproj.row[0].x );

	NUTRACE_RENDER( "PreRender done\n" );

	//glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	//glEnableClientState( GL_COLOR_ARRAY );
}

void nuRenderGL::PostRenderCleanup()
{
	//glDisableVertexAttribArray( VarRectVPos );
	//glDisableVertexAttribArray( VarRectVColor );
	//glDisableVertexAttribArray( VarFillVPos );
	glUseProgram( 0 );
	ActiveProgram = NULL;
}

void nuRenderGL::DrawQuad( const void* v )
{
	NUTRACE_RENDER( "DrawQuad\n" );

	int stride = sizeof(nuVx_PTC);
	const byte* vbyte = (const byte*) v;

	GLint varvpos = 0;
	GLint varvcol = 0;
	GLint varvtex0 = 0;
	GLint vartex0 = 0;
	if ( ActiveProgram == &PRect )
	{
		varvpos = VarRectVPos;
		varvcol = VarRectVColor;
	}
	else if ( ActiveProgram == &PFill )
	{
		varvpos = VarFillVPos;
		varvcol = VarFillVColor;
	}
	else if ( ActiveProgram == &PFillTex )
	{
		varvpos = VarFillTexVPos;
		varvcol = VarFillTexVColor;
		varvtex0 = VarFillTexVUV;
		vartex0 = VarFillTex0;
	}

	glVertexAttribPointer( varvpos, 3, GL_FLOAT, false, stride, vbyte );
	glEnableVertexAttribArray( varvpos );
	
	glVertexAttribPointer( varvcol, 4, GL_UNSIGNED_BYTE, true, stride, vbyte + offsetof(nuVx_PTC, Color) );
	glEnableVertexAttribArray( varvcol );

	if ( varvtex0 != 0 )
	{
		glUniform1i( vartex0, 0 );
		glVertexAttribPointer( varvtex0, 2, GL_FLOAT, true, stride, vbyte + offsetof(nuVx_PTC, UV) );
		glEnableVertexAttribArray( varvtex0 );
	}
	//glVertexPointer( 3, GL_FLOAT, stride, vbyte );
	//glEnableClientState( GL_VERTEX_ARRAY );

	//glTexCoordPointer( 2, GL_FLOAT, stride, vbyte + offsetof(nuVx_PTC, UV) );
	//glColorPointer( GL_BGRA, GL_UNSIGNED_BYTE, stride, vbyte + offsetof(nuVx_PTC, Color) );
	uint16 indices[6];
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 0;
	indices[4] = 2;
	indices[5] = 3;
	glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices );
	//glDrawArrays( GL_TRIANGLES, 0, 3 );

	NUTRACE_RENDER( "DrawQuad done\n" );

	/*
	glBegin( GL_TRIANGLES );
	glColor4f( 1, 0, 0, 1 );
	glVertex3f( 0, 0, 0 );
	glVertex3f( 0, 100, 0 );
	glVertex3f( 100, 0, 0 );
	glEnd();
	*/
	Check();
}

void nuRenderGL::DrawTriangles( int nvert, const void* v, const uint16* indices )
{
	int stride = sizeof(nuVx_PTC);
	const byte* vbyte = (const byte*) v;
	//glVertexPointer( 3, GL_FLOAT, stride, vbyte );
	//glTexCoordPointer( 2, GL_FLOAT, stride, vbyte + offsetof(nuVx_PTC, UV) );
	//glColorPointer( GL_BGRA, GL_UNSIGNED_BYTE, stride, vbyte + offsetof(nuVx_PTC, Color) );
	glDrawElements( GL_TRIANGLES, nvert, GL_UNSIGNED_SHORT, indices );
	Check();
}

void nuRenderGL::LoadTexture( const nuImage* img )
{
	if ( SingleTex2D == 0 )
		glGenTextures( 1, &SingleTex2D );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, SingleTex2D );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, img->GetWidth(), img->GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img->GetData() );
	glGenerateMipmap( GL_TEXTURE_2D );
}

void nuRenderGL::DeleteProgram( nuGLProg& prog )
{
	if ( prog.Prog ) glDeleteShader( prog.Prog );
	if ( prog.Vert ) glDeleteShader( prog.Vert );
	if ( prog.Frag ) glDeleteShader( prog.Frag );
	prog = nuGLProg();
}

bool nuRenderGL::LoadProgram( nuGLProg& prog, const char* vsrc, const char* fsrc )
{
	return LoadProgram( prog.Vert, prog.Frag, prog.Prog, vsrc, fsrc );
}

bool nuRenderGL::LoadProgram( GLuint& vshade, GLuint& fshade, GLuint& prog, const char* vsrc, const char* fsrc )
{
	NUASSERT(glGetError() == GL_NO_ERROR);

	if ( !LoadShader( GL_VERTEX_SHADER, vshade, vsrc ) ) return false;
	if ( !LoadShader( GL_FRAGMENT_SHADER, fshade, fsrc ) ) return false;

	prog = glCreateProgram();

	glAttachShader( prog, vshade );
	glAttachShader( prog, fshade );
	glLinkProgram( prog );

	int ilen;
	const int maxBuff = 8000;
	GLchar ibuff[maxBuff];

	GLint linkStat;
	glGetProgramiv( prog, GL_LINK_STATUS, &linkStat );
	glGetProgramInfoLog( prog, maxBuff, &ilen, ibuff );
	NUTRACE( ibuff );
	if ( ibuff[0] != 0 ) NUTRACE( "\n" );
	if ( linkStat == 0 ) 
		return false;

	return glGetError() == GL_NO_ERROR;
}

bool nuRenderGL::LoadShader( GLenum shaderType, GLuint& shader, const char* src )
{
	NUASSERT(glGetError() == GL_NO_ERROR);

	shader = glCreateShader( shaderType );

	GLchar* vstring[1];
	vstring[0] = (GLchar*) src;

	glShaderSource( shader, 1, (const GLchar**) vstring, NULL );
	
	int ilen;
	const int maxBuff = 8000;
	GLchar ibuff[maxBuff];

	GLint compileStat;
	glCompileShader( shader );
	glGetShaderiv( shader, GL_COMPILE_STATUS, &compileStat );
	glGetShaderInfoLog( shader, maxBuff, &ilen, ibuff );
	//glGetInfoLogARB( shader, maxBuff, &ilen, ibuff );
	NUTRACE( ibuff );
	if ( compileStat == 0 ) 
		return false;

	return glGetError() == GL_NO_ERROR;
}

void nuRenderGL::Check()
{
	int e = glGetError();
	if ( e != GL_NO_ERROR )
	{
		NUTRACE( "glError = %d\n", e );
	}
	//NUASSERT( glGetError() == GL_NO_ERROR );
}

