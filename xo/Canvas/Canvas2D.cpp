#include "pch.h"
#include "Canvas2D.h"
#include "../Image/Image.h"

namespace xo {

Canvas2D::Canvas2D(xo::Image* backingImage)
    : Image(backingImage) {
	if (Image != nullptr && Image->TexFormat == TexFormatRGBA8) {
		RenderBuff.attach((uint8_t*) Image->GetData(), Image->GetWidth(), Image->GetHeight(), Image->TexStride);
		PixFormatRGBA_Pre.attach(RenderBuff);
		RenderBaseRGBA_Pre.attach(PixFormatRGBA_Pre);
		RenderAA_RGBA_Pre.attach(RenderBaseRGBA_Pre);
		IsAlive = Image->GetData() != nullptr && Image->GetWidth() != 0 && Image->GetHeight() != 0;
		InvalidRect.SetInverted();
	} else {
		IsAlive = false;
	}
}

Canvas2D::~Canvas2D() {
}

void Canvas2D::Fill(Color color) {
	if (!IsAlive)
		return;

	FillRect(Box(0, 0, Width(), Height()), color);
}

void Canvas2D::FillRect(Box box, Color color) {
	if (!IsAlive)
		return;

	RenderBaseRGBA_Pre.copy_bar(box.Left, box.Top, box.Right, box.Bottom, ColorToAgg8(color));
	InvalidRect.ExpandToFit(box);
}

void Canvas2D::StrokeRect(Box box, Color color, float linewidth) {
	if (!IsAlive)
		return;

	float v[8] = {
	    (float) box.Left, (float) box.Top,
	    (float) box.Right, (float) box.Top,
	    (float) box.Right, (float) box.Bottom,
	    (float) box.Left, (float) box.Bottom,
	};

	StrokeLine(true, 4, v, 2 * sizeof(float), color, linewidth);
}

void Canvas2D::StrokeLine(bool closed, int nvx, const float* vx, int vx_stride_bytes, Color color, float linewidth) {
	if (!IsAlive)
		return;

	RasAA.reset();
	RasAA.filling_rule(agg::fill_non_zero);

	agg::path_storage path;

	path.start_new_path();

	// emit first vertex
	path.move_to(vx[0], vx[1]);
	(char*&) vx += vx_stride_bytes;
	nvx--;

	// emit remaining vertices
	for (int i = 0; i < nvx; i++, (char *&) vx += vx_stride_bytes)
		path.line_to(vx[0], vx[1]);

	if (closed)
		path.close_polygon();

	TLineClipper                   clipped_line(path);
	TFillClipper                   clipped_fill(path);
	agg::conv_stroke<TLineClipper> clipped_line_stroked(clipped_line);
	agg::conv_stroke<TFillClipper> clipped_fill_stroked(clipped_fill);

	if (closed) {
		clipped_fill.clip_box(-linewidth, -linewidth, Width() + linewidth, Height() + linewidth);
		clipped_fill_stroked.line_cap(agg::butt_cap);
		clipped_fill_stroked.line_join(agg::miter_join);
		clipped_fill_stroked.width(linewidth);
		RasAA.add_path(clipped_fill_stroked);
	} else {
		clipped_line.clip_box(-linewidth, -linewidth, Width() + linewidth, Height() + linewidth);
		clipped_line_stroked.line_cap(agg::butt_cap);
		clipped_line_stroked.line_join(agg::miter_join);
		clipped_line_stroked.width(linewidth);
		RasAA.add_path(clipped_line_stroked);
	}

	RenderAA_RGBA_Pre.color(ColorToAgg8(color));

	RenderScanlines();
}

agg::rgba Canvas2D::ColorToAgg(Color c) {
	return agg::rgba(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}

agg::rgba8 Canvas2D::ColorToAgg8(Color c) {
	return agg::rgba8(c.r, c.g, c.b, c.a);
}

void Canvas2D::RenderScanlines() {
	agg::render_scanlines(RasAA, Scanline, RenderAA_RGBA_Pre);
	InvalidRect.ExpandToFit(Box(RasAA.min_x(), RasAA.min_y(), RasAA.max_x(), RasAA.max_y()));
}
}
