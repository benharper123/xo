#pragma once

#include "../xoDefs.h"
#include "../Text/xoGlyphCache.h"

/* An instance of this is created for each render.
Any state that is persisted between renderings is stored in xoRenderGL.
*/
class XOAPI xoRenderer
{
public:
	// I initially tried to not pass xoDoc in here, but I eventually needed it to lookup canvas objects
	xoRenderResult	Render(const xoDoc* doc, xoImageStore* images, xoStringTable* strings, xoRenderBase* driver, const xoRenderDomNode* root);

protected:
	enum TexUnits
	{
		TexUnit0 = 0,
	};
	enum Corners
	{
		TopLeft,
		BottomLeft,
		BottomRight,
		TopRight,
	};
	const xoDoc*				Doc;
	xoRenderBase*				Driver;
	xoImageStore*				Images;
	xoStringTable*				Strings;
	ohash::set<xoGlyphCacheKey>	GlyphsNeeded;

	void			RenderEl(xoPoint base, const xoRenderDomEl* node);
	void			RenderNode(xoPoint base, const xoRenderDomNode* node);
	void			RenderCornerArcs(Corners corner, float xEdge, float yEdge, xoVec2f outerRadii, float borderWidthX, float borderWidthY, uint32 bgRGBA, uint32 borderRGBA);
	void			RenderQuadratic(xoPoint base, const xoRenderDomNode* node);
	void			RenderText(xoPoint base, const xoRenderDomText* node);
	void			RenderTextChar_WholePixel(xoPoint base, const xoRenderDomText* node, const xoRenderCharEl& txtEl);
	void			RenderTextChar_SubPixel(xoPoint base, const xoRenderDomText* node, const xoRenderCharEl& txtEl);
	void			RenderGlyphsNeeded();

	bool			LoadTexture(xoTexture* tex, TexUnits texUnit);		// Load a texture and reset invalid rectangle
	static float	CircleFrom3Pt(const xoVec2f& a, const xoVec2f& b, const xoVec2f& c, xoVec2f& center, float& radius);
	static xoVec2f	PtOnEllipse(float flipX, float flipY, float a, float b, float theta);

};