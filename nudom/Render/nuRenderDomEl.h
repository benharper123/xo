#pragma once

#include "nuStyle.h"
#include "nuMem.h"

class nuRenderStack;

struct NUAPI nuRenderCharEl
{
	int		Char;
	nuPos	X;
	nuPos	Y;
};

// Element that is ready for rendering
class NUAPI nuRenderDomEl
{
public:
				nuRenderDomEl( nuInternalID id, nuTag tag );

	nuInternalID					InternalID;			// Reference to our original nuDomEl
	nuBox							Pos;				// For rectangles, this is the BorderBox
	nuTag							Tag;
};

class NUAPI nuRenderDomNode : public nuRenderDomEl
{
public:
				nuRenderDomNode( nuInternalID id = nuInternalIDNull, nuTag tag = nuTagBody, nuPool* pool = NULL );

	void		Discard();
	void		SetStyle( nuRenderStack& stack );
	void		SetPool( nuPool* pool );

	nuStyleRender					Style;
	nuPoolArray<nuRenderDomEl*>		Children;
};

class NUAPI nuRenderDomText : public nuRenderDomEl
{
public:
	enum Flag
	{
		FlagSubPixelGlyphs = 1,
	};
				nuRenderDomText( nuInternalID id, nuPool* pool );

	void		SetStyle( nuRenderStack& stack );
	
	bool		IsSubPixel() const { return !!(Flags & FlagSubPixelGlyphs); }

	nuFontID						FontID;
	nuPoolArray<nuRenderCharEl>		Text;
	int								Char;
	nuColor							Color;
	uint8							FontSizePx;
	uint8							Flags;
};
