/*
===========================================================================
OpenGL ES 1.1 stubs for desktop-only immediate-mode entry points
===========================================================================
*/
#ifdef USE_GLES_FIXED

#include "../renderercommon/tr_common.h"
#include "../renderercommon/qgl_es1.h"

#define ES1_MAX_VERTS 4096

#ifndef GL_QUADS
#define GL_QUADS 0x0007
#endif
#ifndef GL_POLYGON
#define GL_POLYGON 0x0009
#endif

typedef struct {
	GLfloat xyz[3];
	GLfloat st[2];
	GLubyte color[4];
} es1Vert_t;

static es1Vert_t es1Verts[ES1_MAX_VERTS];
static int es1VertCount;
static GLenum es1Prim;
static qboolean es1InBegin;
static GLfloat es1CurST[2];
static GLubyte es1CurColor[4];

static void ES1_Flush( void )
{
	GLushort indices[ES1_MAX_VERTS * 3];
	int i, n = 0;

	if ( es1VertCount < 1 ) {
		return;
	}

	switch ( es1Prim )
	{
	case GL_TRIANGLES:
		for ( i = 0; i + 2 < es1VertCount; i += 3 )
		{
			indices[n++] = i;
			indices[n++] = i + 1;
			indices[n++] = i + 2;
		}
		break;
	case GL_TRIANGLE_STRIP:
		for ( i = 0; i + 2 < es1VertCount; i++ )
		{
			if ( i & 1 )
			{
				indices[n++] = i;
				indices[n++] = i + 2;
				indices[n++] = i + 1;
			}
			else
			{
				indices[n++] = i;
				indices[n++] = i + 1;
				indices[n++] = i + 2;
			}
		}
		break;
	case GL_QUADS:
		for ( i = 0; i + 3 < es1VertCount; i += 4 )
		{
			indices[n++] = i;
			indices[n++] = i + 1;
			indices[n++] = i + 2;
			indices[n++] = i;
			indices[n++] = i + 2;
			indices[n++] = i + 3;
		}
		break;
	case GL_LINES:
		for ( i = 0; i + 1 < es1VertCount; i += 2 )
		{
			indices[n++] = i;
			indices[n++] = i + 1;
		}
		break;
	case GL_POLYGON:
		if ( es1VertCount >= 3 )
		{
			for ( i = 1; i < es1VertCount - 1; i++ )
			{
				indices[n++] = 0;
				indices[n++] = i;
				indices[n++] = i + 1;
			}
		}
		break;
	default:
		return;
	}

	if ( n < 1 ) {
		return;
	}

	qglDisableClientState( GL_COLOR_ARRAY );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglTexCoordPointer( 2, GL_FLOAT, 0, es1Verts[0].st );
	qglVertexPointer( 3, GL_FLOAT, sizeof( es1Vert_t ), es1Verts[0].xyz );
	if ( qglColor4ub ) {
		qglColor4ub( es1Verts[0].color[0], es1Verts[0].color[1],
			es1Verts[0].color[2], es1Verts[0].color[3] );
	} else {
		qglColor4f( es1Verts[0].color[0] / 255.0f, es1Verts[0].color[1] / 255.0f,
			es1Verts[0].color[2] / 255.0f, es1Verts[0].color[3] / 255.0f );
	}

	qglDrawElements( GL_TRIANGLES, n, GL_UNSIGNED_SHORT, indices );

	qglEnableClientState( GL_VERTEX_ARRAY );
}

static void APIENTRY GLimp_ES1_Begin( GLenum mode )
{
	es1Prim = mode;
	es1VertCount = 0;
	es1InBegin = qtrue;
	es1CurST[0] = 0.0f;
	es1CurST[1] = 0.0f;
	es1CurColor[0] = 255;
	es1CurColor[1] = 255;
	es1CurColor[2] = 255;
	es1CurColor[3] = 255;
}

static void APIENTRY GLimp_ES1_End( void )
{
	if ( es1InBegin ) {
		ES1_Flush();
	}
	es1InBegin = qfalse;
	es1VertCount = 0;
}

static void APIENTRY GLimp_ES1_Vertex3f( GLfloat x, GLfloat y, GLfloat z )
{
	es1Vert_t *v;

	if ( !es1InBegin || es1VertCount >= ES1_MAX_VERTS ) {
		return;
	}

	v = &es1Verts[es1VertCount++];
	v->xyz[0] = x;
	v->xyz[1] = y;
	v->xyz[2] = z;
	v->st[0] = es1CurST[0];
	v->st[1] = es1CurST[1];
	v->color[0] = es1CurColor[0];
	v->color[1] = es1CurColor[1];
	v->color[2] = es1CurColor[2];
	v->color[3] = es1CurColor[3];
}

static void APIENTRY GLimp_ES1_Vertex3fv( const GLfloat *v )
{
	GLimp_ES1_Vertex3f( v[0], v[1], v[2] );
}

static void APIENTRY GLimp_ES1_Vertex2f( GLfloat x, GLfloat y )
{
	GLimp_ES1_Vertex3f( x, y, 0.0f );
}

static void APIENTRY GLimp_ES1_TexCoord2f( GLfloat s, GLfloat t )
{
	if ( es1InBegin ) {
		es1CurST[0] = s;
		es1CurST[1] = t;
	}
}

static void APIENTRY GLimp_ES1_TexCoord2fv( const GLfloat *v )
{
	es1CurST[0] = v[0];
	es1CurST[1] = v[1];
}

static void APIENTRY GLimp_ES1_Color3f( GLfloat r, GLfloat g, GLfloat b )
{
	if ( es1InBegin ) {
		es1CurColor[0] = (GLubyte)( r * 255.0f );
		es1CurColor[1] = (GLubyte)( g * 255.0f );
		es1CurColor[2] = (GLubyte)( b * 255.0f );
		es1CurColor[3] = 255;
	} else {
		qglColor4f( r, g, b, 1.0f );
	}
}

static void APIENTRY GLimp_ES1_Color4ubv( const GLubyte *v )
{
	es1CurColor[0] = v[0];
	es1CurColor[1] = v[1];
	es1CurColor[2] = v[2];
	es1CurColor[3] = v[3];
}

static void APIENTRY GLimp_ES1_MultiTexCoord2fARB( GLenum unit, GLfloat s, GLfloat t )
{
	if ( !es1InBegin ) {
		return;
	}

	if ( qglClientActiveTextureARB ) {
		if ( unit <= 1 ) {
			qglClientActiveTextureARB( GL_TEXTURE0_ARB + unit );
		} else {
			qglClientActiveTextureARB( unit );
		}
	}
	es1CurST[0] = s;
	es1CurST[1] = t;
}

static void APIENTRY GLimp_ES1_ArrayElement( GLint index )
{
	(void)index;
}

void GLimp_AssignES1DesktopStubs( void )
{
	qglBegin = GLimp_ES1_Begin;
	qglEnd = GLimp_ES1_End;
	qglVertex2f = GLimp_ES1_Vertex2f;
	qglVertex3f = GLimp_ES1_Vertex3f;
	qglVertex3fv = GLimp_ES1_Vertex3fv;
	qglTexCoord2f = GLimp_ES1_TexCoord2f;
	qglTexCoord2fv = GLimp_ES1_TexCoord2fv;
	qglColor3f = GLimp_ES1_Color3f;
	qglColor4ubv = GLimp_ES1_Color4ubv;
	qglArrayElement = GLimp_ES1_ArrayElement;
	qglMultiTexCoord2fARB = GLimp_ES1_MultiTexCoord2fARB;

	if ( !qglActiveTextureARB ) {
		qglActiveTextureARB = (void (APIENTRY *)(GLenum))glActiveTexture;
	}
	if ( !qglClientActiveTextureARB ) {
		qglClientActiveTextureARB = (void (APIENTRY *)(GLenum))glClientActiveTexture;
	}
}

#endif /* USE_GLES_FIXED */
