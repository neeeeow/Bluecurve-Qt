/*
  Bluecurve Qt5/6 style.

  Copyright (c) 2025-2026 neeeeow
  Author: neeeeow (https://github.com/neeeeow/Bluecurve-Qt)

  Based on the Bluecurve Qt3 style:
  Copyright (c) 2002 Red Hat, Inc.
  Authors: Bernhard Rosenkränzer <bero@redhat.com>
           Preston Brown <pbrown@redhat.com>
           Than Ngo <than@redhat.com>
           Alexander Larsson <alexl@redhat.com>
           Chris Lee <clee@redhat.com>
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/  

#include "bluecurvestyle.h"

#include <algorithm>

#include <QStyleFactory>
#include <QStyleOption>
#include <QPushButton>
#include <QPointer>
#include <QEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QComboBox>
#include <QScrollBar>
#include <QProgressBar>
#include <QCheckBox>
#include <QRadioButton>
#include <QGuiApplication>
#include <QBitmap>

#define RADIO_SIZE 13
#define CHECK_SIZE 13
#define DARK_FACTOR 0.7
#define DISABLED_ICON_SATURATION 0.8

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define CLAMP_UCHAR(v) ((unsigned char) (CLAMP (((int)v), (int)0, (int)255)))

#include "bits.h"

const double BluecurveStyle::shadeFactors[8] = {1.065, 0.963, 0.896, 0.85, 0.768, 0.665, 0.4, 0.205};

static qreal
getDpr(const QPainter *p)
{		
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return p->device() ? p->device()->devicePixelRatio() : 1.0;
#else
	return p->device() ? p->device()->devicePixelRatioF() : 1.0;
#endif
}

// Scales a QRect for drawing pixel-perfect lines on HiDPI displays
static QRect
getScaledRect(const QRect &rect, const qreal dpr)
{
	return QRect(qRound(rect.x() * dpr), qRound(rect.y() * dpr), rect.width() * dpr, rect.height() * dpr);
}

static void
shade (const QColor &ca, QColor &cb, double k)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	float h, s, l;
#else
	qreal h, s, l;
#endif
	
	ca.getHslF(&h, &s, &l);

	s *= k;
	l *= k;

	cb.setHslF(h, qBound(0.0,s,1.0), qBound(0.0,l,1.0));
}

static QImage *
generate_bit (unsigned char alpha[], const QColor &color, double mult)
{
	 unsigned int r, g, b;
	 QImage *image;
	 QRgb *pixels;
	 int w, h;
	 int x, y;
  
	 r = (int) (color.red() * mult);
	 r = std::min<int>(r, 255);
	 g = (int) (color.green() * mult);
	 g = std::min<int>(g, 255);
	 b = (int) (color.blue() * mult);
	 b = std::min<int>(b, 255);
  
	 image = new QImage (RADIO_SIZE, RADIO_SIZE, QImage::Format_ARGB32);

	 w = image->width();
	 h = image->height();

	 for (y=0; y < h; y++)
	 {
		  pixels = (QRgb *)image->scanLine(y);
		  for (x=0; x < w; x++)
			   pixels[x] = qRgba (r, g, b, alpha?alpha[y*w+x]:255);
	 }

	 return image;
}

static QImage *
colorize_bit (unsigned char *bit,
              unsigned char *alpha,
              const QColor  &new_color)
{
	 QImage *image;
	 double intensity;
	 int x, y;
	 const unsigned char *src, *asrc;
	 QRgb *dest;
  
	 image = new QImage (RADIO_SIZE, RADIO_SIZE, QImage::Format_ARGB32);

	 if (image == NULL)
		  return NULL;
  
	 for (y = 0; y < RADIO_SIZE; y++)
	 {
		  src = bit + y * RADIO_SIZE;
		  asrc = alpha + y * RADIO_SIZE;
		  dest = (QRgb *)image->scanLine (y);

		  for (x = 0; x < RADIO_SIZE; x++)
		  {
			   int dr, dg, db;
          
			   intensity = src[x]/ 255.0;

			   if (intensity <= 0.5)
			   {
					/* Go from black at intensity = 0.0 to new_color at intensity = 0.5 */
					dr = int((new_color.red() * intensity * 2.0));
					dg = int((new_color.green() * intensity * 2.0));
					db = int((new_color.blue() * intensity * 2.0));
			   }
			   else
			   {
					/* Go from new_color at intensity = 0.5 to white at intensity = 1.0 */
					dr = int((new_color.red() + (255 - new_color.red()) * (intensity - 0.5) * 2.0));
					dg = int((new_color.green() + (255 - new_color.green()) * (intensity - 0.5) * 2.0));
					db = int((new_color.blue() + (255 - new_color.blue()) * (intensity - 0.5) * 2.0));
			   }

			   dest[x] = qRgba (CLAMP_UCHAR (dr), CLAMP_UCHAR (dg), CLAMP_UCHAR (db), asrc[x]);
		  }
	 }

	 return image;
}

static void
composeImage (QImage *destImg, QImage *srcImg)
{
	 int w, h, x, y;
	 QRgb *src, *dest;
	 unsigned int a;
	 QRgb s, d;

	 w = destImg->width();
	 h = destImg->height();

	 for (y = 0; y < h; y++)
	 {
		  src = (QRgb *)srcImg->scanLine(y);
		  dest = (QRgb *)destImg->scanLine(y);

		  for (x = 0; x < w; x++)
		  {
			   s = src[x];
			   d = dest[x];
          
			   a = qAlpha(s);

			   dest[x] = qRgba ((qRed(s) * a + (255-a)*qRed(d)) / 255,
								(qGreen(s) * a + (255-a)*qGreen(d)) / 255,
								(qBlue(s) * a + (255-a)*qBlue(d)) / 255,
								a + ((255-a)*qAlpha(d)) / 255);
		  }
	 }
}

/* We assume this seldom collides, since we can only cache one at a time */
static long
hashColorGroup (const QPalette &palette)
{
	return palette.button().color().rgb() << 8 ^ palette.highlight().color().rgb();
}

static QPixmap
pixmap_saturate_and_pixelate(const QPixmap &src,
							 qreal saturation,
							 bool pixelate)
{
	/* Adapted from GDK source */
	
	if (saturation == 1.0 && !pixelate)
		return src;

	// Convert pixmaps to images
	QImage src_img(src.toImage());
	QImage dest_img(src_img);

	// Convert to Format_ARGB32, to make manipulation easier
	src_img.convertTo(QImage::Format_ARGB32);
	dest_img.convertTo(QImage::Format_ARGB32);

	int width = src_img.width();
	int height = src_img.height();
	
	for (int i = 0; i < height; i++) {
		// best practice to cast to QRgb, per Qt documentation
	    const QRgb *src_line = reinterpret_cast<const QRgb *>(src_img.constScanLine(i));
	    QRgb *dest_line = reinterpret_cast<QRgb *>(dest_img.scanLine(i));

		for (int j = 0; j < width; j++) {
			QRgb src_pixel = src_line[j];
			int dest_r, dest_g, dest_b, dest_a; // dest QRgb values
			
			int intensity = (int)(qRed(src_pixel) * 0.3 + qGreen(src_pixel) * 0.59 + qBlue(src_pixel) * 0.11);
			auto saturate = [saturation, intensity](int v) {
				return (1.0 - saturation) * intensity + saturation * v;
			};

			if (pixelate && (i + j) % 2 == 0) {
				dest_r = intensity / 2 + 127;
				dest_g = intensity / 2 + 127;
				dest_b = intensity / 2 + 127;
			} else if (pixelate) {
				dest_r = qBound(0, (int)(saturate(qRed(src_pixel)) * DARK_FACTOR), 255);
				dest_g = qBound(0, (int)(saturate(qGreen(src_pixel)) * DARK_FACTOR), 255);
				dest_b = qBound(0, (int)(saturate(qBlue(src_pixel)) * DARK_FACTOR), 255);
			} else {
				dest_r = qBound(0, (int)saturate(qRed(src_pixel)), 255);
				dest_g = qBound(0, (int)saturate(qGreen(src_pixel)), 255);
				dest_b = qBound(0, (int)saturate(qBlue(src_pixel)), 255);
			}

			dest_a = qAlpha(src_pixel);
			dest_line[j] = qRgba(dest_r, dest_b, dest_g, dest_a);
		}
	}
	
	return QPixmap::fromImage(dest_img);
}

BluecurveStyle::BluecurveColorData::~BluecurveColorData()
{
	int i;

	for (i = 0; i < 8; i++) {
		if (radioPix[i] != 0)
			delete radioPix[i];
	}
	
	if (radioMask != 0)
		delete radioMask;

	for (i = 0; i < 6; i++) {
		if (checkPix[i] != 0)
			delete checkPix[i];
	}
}

void
BluecurveStyle::polish(QWidget *widget)
{
	if (widget->inherits("QAbstractButton") ||
		widget->inherits("QComboBox") ||
		widget->inherits("QSplitterHandle") ||
		widget->inherits("QSpinBox") ||
		widget->inherits("QDateTimeEdit"))
		widget->setAttribute(Qt::WA_Hover, true);

	if (widget->inherits("QScrollBar") ||
		widget->inherits("QAbstractSlider")) {
		widget->setMouseTracking(true);
		widget->setAttribute(Qt::WA_Hover, true);
	}
	
	QCommonStyle::polish(widget);
}

BluecurveStyle::BluecurveColorData *
BluecurveStyle::realizeData (const QPalette &palette) const
{
	BluecurveColorData *cdata;
	int i, j;

	cdata = new BluecurveColorData;
	cdata->buttonColor = palette.button().color().rgb();
	cdata->spotColor = palette.highlight().color().rgb();

	for (i = 0; i < 8; i++) { // precompute the shade colors
		shade (palette.button().color(), cdata->btnShades[i], shadeFactors[i]);
		shade (palette.window().color(), cdata->bgShades[i], shadeFactors[i]);
	}

	shade (palette.highlight().color(), cdata->spots[0], 1.62);
	shade (palette.highlight().color(), cdata->spots[1], 1.05);
	shade (palette.highlight().color(), cdata->spots[2], 0.72);

	QImage *dot, *inconsistent, *outline, *circle, *check, *base;

	dot = colorize_bit (dot_intensity, dot_alpha, palette.highlight().color());
	outline = generate_bit (outline_alpha, cdata->btnShades[6], 1.0);

	QImage composite (RADIO_SIZE, RADIO_SIZE, QImage::Format_ARGB32);

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			if (i == 0) {
				composite.fill (palette.button().color().rgb());
			} else {
				composite.fill (palette.midlight().color().rgb());
			}
			composeImage (&composite, outline);

			if (j == 0) {
				circle = generate_bit (circle_alpha, QColor(Qt::white), 1.0);
			} else {
				circle = generate_bit (circle_alpha, cdata->btnShades[1], 1.0);
			}

			composeImage (&composite, circle);
			delete circle;

			cdata->radioPix[i*4+j*2+0] = new QPixmap (QPixmap::fromImage(composite));

			composeImage (&composite, dot);
			cdata->radioPix[i*4+j*2+1] = new QPixmap (QPixmap::fromImage(composite));
		}
	}

	QImage mask = outline->createAlphaMask();
	cdata->radioMask = new QBitmap (QBitmap::fromImage(mask));

	check = generate_bit (check_alpha, palette.highlight().color(), 1.0);
	inconsistent = generate_bit (check_inconsistent_alpha, palette.highlight().color(), 1.0);

	for (i = 0; i < 2; i++) {
		if (i == 0) {
			base = generate_bit (check_base_alpha, QColor(Qt::white), 1.0);
		} else {
			base = generate_bit (check_base_alpha, cdata->btnShades[1], 1.0);
		}

		composite.fill (cdata->btnShades[6].rgb());
		composeImage (&composite, base);
		cdata->checkPix[i*3+0] = new QPixmap (QPixmap::fromImage(composite));

		composeImage (&composite, check);
		cdata->checkPix[i*3+1] = new QPixmap (QPixmap::fromImage(composite));

		composite.fill (cdata->btnShades[6].rgb());
		composeImage (&composite, base);
		composeImage (&composite, inconsistent);
		cdata->checkPix[i*3+2] = new QPixmap (QPixmap::fromImage(composite));

		delete base;
	}	

    // GTK check marks - 0 is highlighted, 1 is normal 
	check = generate_bit (checkmark, palette.highlightedText().color(), 1.0);
	cdata->checkMark[0] = new QPixmap (QPixmap::fromImage(*check));
	check = generate_bit (checkmark, palette.buttonText().color(), 1.0);
	cdata->checkMark[1] = new QPixmap (QPixmap::fromImage(*check));

	delete dot;
	delete inconsistent;
	delete outline;
	delete check;
	
	return cdata;
}

const BluecurveStyle::BluecurveColorData *
BluecurveStyle::lookupData (const QPalette &palette) const
{

	BluecurveColorData *cdata;
	long h;
	QCache<long, BluecurveColorData> *cache;

	h = hashColorGroup(palette);

	// Doing this cast seems very wrong but the original theme does it this way,
	// and it doesn't affect compilation... so who cares, right?
	cache = (QCache<long, BluecurveColorData> *)&m_dataCache;

	cdata = cache->object(h);

	if (cdata == 0 || !cdata->isGroup(palette)) {
		if (cdata != 0) {
			cache->remove (h);
		}

		cdata = realizeData (palette);
		cache->insert (h, cdata);
	}

	return cdata;	
	
}

void
BluecurveStyle::drawTextRect(QPainter *p, const QStyleOption *opt,
							 const QBrush *fill) const
{
	p->save();
	const qreal dpr = getDpr(p);
	QRect r;
	bool isScaled = false;
	if (!qFuzzyCompare(dpr, qreal(1))) {
		const qreal inverseScale = qreal(1) / dpr;
		p->scale(inverseScale, inverseScale);
		p->translate(0.5, 0.5);
		isScaled = true;
		r = getScaledRect(opt->rect, dpr);
	} else {
		r = opt->rect;
	}
	
	QRect br = r;

	const BluecurveColorData *cdata = lookupData(opt->palette);

	p->setPen(cdata->btnShades[5]);
	p->drawRect(r.adjusted(0,0,-1,-1));

	// button bevel
	p->setPen(opt->palette.light().color());
	p->drawLine(r.x() + r.width() - 2, r.y() + 2,
				r.x() + r.width() - 2, r.y() + r.height() - 3); // right
	p->drawLine(r.x() + 2, r.y() + r.height() - 2,
				r.x() + r.width() - 2, r.y() + r.height() - 2); // bottom

	p->setPen(cdata->btnShades[1]);
	p->drawLine(r.x() + 1, r.y() + 2,
				r.x() + 1, r.y() + r.height() - 2); // left
	p->drawLine(r.x() + 1, r.y() + 1,
				r.x() + r.width() - 2, r.y() + 1); // top

	br.adjust(2, 2, -2, -2);

	// fill
	if (fill) {
		if (isScaled)
			p->translate(-0.5, -0.5);
		p->fillRect(br, *fill);
	}
	p->restore();
}

void
BluecurveStyle::drawLightBevel(QPainter *p, const QStyleOption *opt,
							   const QBrush *fill, bool btnPal, bool dark) const
{
	// Set up QPainter and drawing rectangle for HiDPI scaling if necessary
	const qreal dpr = getDpr(p);
	QRect r;
	bool isScaled = false;
	p->save();
	if (!qFuzzyCompare(dpr, qreal(1))) {
		const qreal inverseScale = qreal(1) / dpr;
		p->scale(inverseScale, inverseScale);
		p->translate(0.5, 0.5);
		isScaled = true;
	    r = getScaledRect(opt->rect, dpr);
	} else {
		r = opt->rect;
	}

	// Draw a sunken frame if the appropriate flags are set, otherwise draw
	// a raised frame by default
	bool sunken = (opt->state & (State_On | State_Sunken));

	// Color data for shades
	const BluecurveColorData *cdata = lookupData(opt->palette);

	// Outside border
	if (btnPal)
		p->setPen(dark ? cdata->btnShades[6] : cdata->btnShades[5]);
	else
		p->setPen(dark ? cdata->bgShades[6] : cdata->bgShades[5]);
    p->drawRect(r.adjusted(0, 0, -1, -1));

	// Draw the bevel (NB: the white line always overlaps the grey)
	int x1 = r.x()+1; int y1 = r.y()+1; int x2=r.x() + r.width() - 2; int y2=r.y() + r.height() - 2;
	if (sunken) {
		p->setPen(btnPal ? cdata->btnShades[2] : cdata->bgShades[2]);
		p->drawLine(x1,y1,x1,y2); // left
		p->drawLine(x1,y1,x2,y1); // top
		p->setPen(Qt::white);
		p->drawLine(x1,y2,x2,y2); // bottom
		p->drawLine(x2,y1,x2,y2); // right
	} else {
		p->setPen(btnPal ? cdata->btnShades[2] : cdata->bgShades[2]);
		p->drawLine(x1,y2,x2,y2); // bottom
		p->drawLine(x2,y1,x2,y2); // right
		p->setPen(Qt::white);
		p->drawLine(x1,y1,x1,y2); // left
		p->drawLine(x1,y1,x2,y1); // top
	}
	
	// Fill
	if (fill) {
		if (isScaled)
			p->translate(-0.5, -0.5);		
		p->fillRect(r.adjusted(2, 2, -2, -2), *fill);
	}
	p->restore();
}

void
BluecurveStyle::drawItemText(QPainter *painter, const QRect &rect, int alignment, const QPalette &pal,
							 bool enabled, const QString& text, QPalette::ColorRole textRole) const
{
	if (text.isEmpty())
		return;
	QPen savedPen;
	if (textRole != QPalette::NoRole) {
		savedPen = painter->pen();
		painter->setPen(QPen(pal.brush(textRole), savedPen.widthF()));
	}
	if (!enabled) {
		if (proxy()->styleHint(SH_DitherDisabledText)) {
			QRect br;
			painter->drawText(rect, alignment, text, &br);
			painter->fillRect(br, QBrush(painter->background().color(), Qt::Dense5Pattern));
			return;
		} else if (proxy()->styleHint(SH_EtchDisabledText)) {
			QPen pen = painter->pen();
			painter->setPen(Qt::white); // Compared to QStyle's default implementation, Bluecurve *always* uses white etching
			painter->drawText(rect.adjusted(1, 1, 1, 1), alignment, text);
			painter->setPen(pen);
		}
	}
	painter->drawText(rect, alignment, text);
	if (textRole != QPalette::NoRole)
		painter->setPen(savedPen);
}

void
BluecurveStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
							  QPainter *p, const QWidget *widget) const
{

	const qreal dpr = getDpr(p);
	const qreal inverseScale = qreal(1) / dpr;
	QRect r; // Drawing rectangle for elements which must not be scaled
	bool isScaled = false;
	if (!qFuzzyCompare(dpr, qreal(1))) {
		isScaled = true;
	    r = getScaledRect(opt->rect, dpr);
	} else {
		r = opt->rect;
	}

	const BluecurveColorData *cdata = lookupData(opt->palette);
	if (!cdata)
		return;
	
	switch (pe) {
	// BUTTONS
	// -------------------------------------------------------------------
	case PE_PanelButtonCommand:
	case PE_PanelButtonBevel:
	case PE_PanelButtonTool:
	case PE_IndicatorButtonDropDown: {
		const QStyleOptionButton *button = qstyleoption_cast<const QStyleOptionButton *>(opt);
		if (!button)
			break;
		bool enabled = opt->state & State_Enabled;
		
		const QBrush *fill;

		if (enabled && (button->state & State_Sunken))
			fill = &opt->palette.mid();
		else if (enabled && (button->state & State_MouseOver))
			fill = &opt->palette.midlight();
		else if (enabled && (button->state & State_On))
			fill = &opt->palette.mid();
		else // flat buttons should never be filled in
			fill = (button->features & QStyleOptionButton::Flat) ? nullptr : &opt->palette.button();

		if (fill) // buttons with no fill should have no border
			drawLightBevel(p, button, fill, true, true);

		if (button->features & QStyleOptionButton::DefaultButton) {
			p->save();
			if (isScaled) {
				p->scale(inverseScale, inverseScale);
				p->translate(0.5, 0.5);
			}				
			p->setPen(Qt::black);
			p->drawRect(r.adjusted(0,0,-1,-1));
			p->restore();
		}

		break;
	}
		
	case PE_FrameButtonBevel:
	case PE_FrameButtonTool: {
		drawLightBevel(p, opt, 0, true, true);
		break;
	}
		
	case PE_FrameDefaultButton: {
		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}
		p->setPen(opt->palette.shadow().color());
		p->setBrush(Qt::NoBrush);
		p->drawRect(r.adjusted(0,0,-1,-1));
		p->restore();
		break;
	}

	// CLOSE TAB BUTTON
	// -------------------------------------------------------------------
	case PE_IndicatorTabClose: {
		// Set button options
		QStyleOptionButton button;
	    button.QStyleOption::operator=(*opt);
		button.features = QStyleOptionButton::Flat;

		// Draw the button
		proxy()->drawPrimitive(PE_PanelButtonTool, &button, p, widget);

		// Draw icon
		const int iconWidth(proxy()->pixelMetric(QStyle::PM_SmallIconSize, opt, widget));
		const QIcon icon = QIcon::fromTheme(QStringLiteral("tab-close"));
		QIcon::State state = (opt->state & State_On) ? QIcon::On : QIcon::Off;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		QPixmap pixmap = icon.pixmap(QSize(iconWidth, iconWidth), dpr, QIcon::Normal, state);
#else
		QPixmap pixmap = icon.pixmap(widget ? widget->window()->windowHandle() : nullptr, QSize(iconWidth, iconWidth), QIcon::Normal, state);
#endif
		if (!(opt->state & State_Enabled))
			pixmap = pixmap_saturate_and_pixelate(pixmap, DISABLED_ICON_SATURATION, true);
		
	    proxy()->drawItemPixmap(p, button.rect, Qt::AlignCenter, pixmap);
		
		break;
	}
		
	// CHECK/RADIO INDICATORS
	// -------------------------------------------------------------------
	case PE_IndicatorMenuCheckMark: {
		p->save();
		if (isScaled)
			p->scale(inverseScale, inverseScale);
		QPoint qp = QPoint(r.center().x() - RADIO_SIZE/4, 
						   r.center().y() - RADIO_SIZE/2);
		if (opt->state & State_Selected)
			p->drawPixmap(qp, *(cdata->checkMark[0]));
		else
			p->drawPixmap(qp, *(cdata->checkMark[1]));

		p->restore();
		break;		
	}

	case PE_IndicatorCheckBox: {
		int pix = 0;
		if (opt->state & State_Sunken)
			pix += 3;
			
		if (opt->state & State_On)
			pix += 1;
		else if (opt->state & State_NoChange)
			pix += 2;

		QPixmap checkPix = *cdata->checkPix[pix];
		const int offset = r.width()/2 - checkPix.width()/2;

		p->save();
		if (isScaled)
			p->scale(inverseScale, inverseScale);
		p->drawPixmap(r.x() + offset, r.y() + offset, checkPix);
		p->restore();
		break;
	}

	case PE_IndicatorRadioButton: {
		QPixmap radio(13,13); // Pixmap for drawing the radio button
		QPainter radioPainter(&radio);

		int pix = 0;
		if (opt->state & State_MouseOver) {
			pix += 4;
			radio.fill(opt->palette.brush(QPalette::Midlight).color());
		} else {
			radio.fill(opt->palette.brush(QPalette::Window).color());
		}

		if (opt->state & State_Sunken)
			pix += 2;
		if (opt->state & State_On)
			pix += 1;
		
		radioPainter.drawPixmap(0,0, *cdata->radioPix[pix]);
		radioPainter.end();
		radio.setMask(*cdata->radioMask);
		
		p->save();
		if (isScaled)
			p->scale(inverseScale, inverseScale);

		int offset = r.width() / 2 - radio.width() / 2;
		
		p->drawPixmap(r.x() + offset, r.y() + offset, radio);
		p->restore();
		break;			
	}

	// SEPARATORS AND HANDLES
	// -------------------------------------------------------------------
	case PE_IndicatorToolBarHandle: {
		// Background fill
		p->fillRect(opt->rect, opt->palette.window());		
		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}
		QStyle::State state = opt->state;
		state |= State_Raised;

		if (state & State_Horizontal) {
			int xx = r.left() + (r.width() - 4) / 2;
			int yy = r.top() + 3;
			int nLines = (r.height() - 6) / 5;

			for (int i = 0; i < nLines; yy += 5, i++) {
				p->setPen(cdata->bgShades[5]);
				p->drawLine(xx, yy + 3, xx + 3, yy);
				p->setPen(Qt::white);
				p->drawLine(xx, yy + 4, xx + 3, yy + 1);
			}			
		} else {
			int xx = r.left() + 3;
			int yy = r.top() + (r.height() - 4) / 2;
			int nLines = (r.width() - 6) / 4;

			for (int i = 0; i < nLines; xx += 5, i++) {
				p->setPen(cdata->bgShades[5]);
				p->drawLine(xx + 3, yy,
							xx, yy + 3);
				p->setPen(Qt::white);
				p->drawLine(xx + 3, yy + 1,
							xx + 1, yy + 4);
			}
		}

		p->restore();
		break;
	}

	case PE_IndicatorToolBarSeparator: {
		if (r.width() > 20 || r.height() > 20) {
			p->save();
			if (isScaled) {
				p->scale(inverseScale, inverseScale);
				p->translate(0.5, 0.5);
			}
			if (opt->state & State_Horizontal) {
				p->setPen(cdata->bgShades[5]);
				p->drawLine(r.left() + 1, r.top() + 6, r.left() + 1, r.bottom() - 6);
				p->setPen(cdata->bgShades[3]);
				p->drawLine(r.left() + 2, r.top() + 6, r.left() + 2, r.bottom() - 6);
			} else {
				p->setPen(cdata->bgShades[5]);
				p->drawLine(r.left() + 6, r.top() + 1, r.right() - 6, r.top() + 1);
				p->setPen(cdata->bgShades[3]);
				p->drawLine(r.left() + 6, r.top() + 2, r.right() - 6, r.top() + 2);
			}
			p->restore();
		} else {
		    QCommonStyle::drawPrimitive(pe, opt, p, widget);
		}

		break;		
	}

	case PE_IndicatorDockWidgetResizeHandle: {
		QStyleOption dockWidgetHandle(*opt);
		bool horizontal = opt->state & State_Horizontal;
        dockWidgetHandle.state.setFlag(State_Horizontal, !horizontal);
		proxy()->drawControl(CE_Splitter, &dockWidgetHandle, p, widget);
		break;
	}

	// FRAMES
	// -------------------------------------------------------------------
	case PE_Frame:
	case PE_FrameWindow:
	case PE_FrameDockWidget:
	case PE_FrameMenu:
	case PE_FrameTabWidget: {
		if (pe == PE_FrameTabWidget)
			drawLightBevel(p, opt, nullptr, false, true);
		else		
			drawLightBevel(p, opt);
		break;
	}  		

	case PE_FrameTabBarBase: {
		const QStyleOptionTabBarBase *tbb = qstyleoption_cast<const QStyleOptionTabBarBase *>(opt);
		if (!tbb)
			break;

		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}

		// For the tab bar base, simply draw the side of the light bevel facing the tab bar
		switch (tbb->shape) {
		case QTabBar::RoundedNorth:
		case QTabBar::TriangularNorth: {
			p->setPen(cdata->bgShades[6]);
			p->drawLine(r.left(), r.top(), r.right(), r.top());
			p->setPen(Qt::white);
			p->drawLine(r.left(), r.top() + 1, r.right(), r.top() + 1);
			break;
		}
		case QTabBar::RoundedSouth:
		case QTabBar::TriangularSouth: {
			p->setPen(cdata->bgShades[6]);
			p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
			p->setPen(cdata->bgShades[2]);
			p->drawLine(r.left(), r.bottom() - 1, r.right(), r.bottom() - 1);
			break;
		}
		case QTabBar::RoundedWest:
		case QTabBar::TriangularWest: {
			p->setPen(cdata->bgShades[6]);
			p->drawLine(r.left(), r.top(), r.left(), r.bottom());
			p->setPen(Qt::white);
			p->drawLine(r.left() + 1, r.top(), r.left() + 1, r.bottom());
			break;
		}
		case QTabBar::RoundedEast:
		case QTabBar::TriangularEast: {
			p->setPen(cdata->bgShades[6]);
			p->drawLine(r.right(), r.top(), r.right(), r.bottom());
			p->setPen(cdata->bgShades[2]);
			p->drawLine(r.right() - 1, r.top(), r.right() - 1, r.bottom());
			break;		   
		}
		default:
			break;
		}

		p->restore();
		break;
	}

	case PE_FrameGroupBox: {
		const QStyleOptionFrame *frame = qstyleoption_cast<const QStyleOptionFrame *>(opt);
		if (!frame)
			break;

		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}

		if (frame->features & QStyleOptionFrame::Flat) {
			// If the frame is flat, draw only the top part
			p->setPen(cdata->bgShades[3]);
			p->drawLine(r.left(), r.y(), r.right(), r.y());
			p->setPen(cdata->bgShades[0]);
			p->drawLine(r.left(), r.y() + 1, r.right(), r.y() + 1);			
		} else {
			// Dark part
			p->setPen(cdata->bgShades[3]);
			p->drawRect(r.adjusted(0, 0, -2, -2));

			// Light part
			p->setPen(cdata->bgShades[0]);
			p->drawLine(r.x() + 1, r.y() + 1, r.x() + 1, r.y() + r.height() - 3);
			p->drawLine(r.x() + 2, r.y() + 1, r.x() + r.width() - 3, r.y() + 1);
			p->drawLine(r.x(), r.y() + r.height() - 1, r.x() + r.width() - 1, r.y() + r.height() - 1);
			p->drawLine(r.x() + r.width() - 1, r.y(), r.x() + r.width() - 1, r.y() + r.height() - 2);
		}

		p->restore();
		
		break;
	}
		
	case PE_FrameFocusRect: {

		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}
		
		p->setPen(Qt::black);
		int rw = r.width(), rh = r.height(), rx = r.x(), ry = r.y();
		for (int x = 0; x < rw; x += 2) { // Top line
			p->drawPoint(rx + x, ry);
		}
		for (int y = 0; y < rh; y += 2) { // Left line
			p->drawPoint(rx, ry + y);
		}
		if (rw & 1) { // Odd width, top line one pixel short
			for (int y = 2; y < rh; y += 2) { // Right line
				p->drawPoint(rx + rw - 1, ry + y);
			}
		} else { // Even width, top line symmetrical
			for (int y = 1; y < rh; y += 2) { // Right line
				p->drawPoint(rx + rw - 1, ry + y);
			}
		}
		if (rh & 1) { // Odd height
			for (int x = 0; x < rw; x += 2) { // Bottom line
				p->drawPoint(rx + x, ry + rh - 1);
			}
		} else {
			for (int x = 1; x < rw; x += 2) { // Bottom line
				p->drawPoint(rx + x, ry + rh - 1);
			}
		}

		p->restore();
		
		break;
	}

	// PANELS
	// -------------------------------------------------------------------
	case PE_PanelLineEdit: {
		drawTextRect(p, opt, &opt->palette.base());
		break;
	}

	case PE_PanelMenu: {
		p->fillRect(opt->rect, opt->palette.window());
		break;
	}
	
	case PE_PanelMenuBar: {
		p->fillRect(opt->rect, opt->palette.button());
		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}
		
		p->setPen(cdata->btnShades[3]);
		p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());

		p->restore();
		
		break;
	}

	case PE_PanelStatusBar: {
		p->fillRect(opt->rect, opt->palette.window());
		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}
		
		p->setPen(cdata->bgShades[3]);
		p->drawLine(r.left(), r.top(), r.right(), r.top());
		p->setPen(cdata->bgShades[0]);
	    p->drawLine(r.left(), r.top()+1, r.right(), r.top()+1);

		p->restore();
		
		break;
	}

	// ARROWS
	// -------------------------------------------------------------------
	case PE_IndicatorArrowUp:
	case PE_IndicatorArrowDown:
	case PE_IndicatorArrowRight:
	case PE_IndicatorArrowLeft: {
		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}
		
		// get button geometry
		// add 1px of margin to make room for arrow shadow
		int x,y,width,height;
		r.adjusted(1,1,-1,-1).getRect(&x, &y, &width, &height);

		calculate_arrow_geometry(pe, x, y, width, height);

		if ( opt->state & State_Enabled )
			p->setPen( opt->state & State_Selected ? opt->palette.highlightedText().color() : cdata->btnShades[7]);
		else {
			p->setPen(Qt::white);
			drawArrow(p, pe, x+1, y+1, width, height);
			p->setPen(opt->palette.buttonText().color());
		}

		drawArrow(p, pe, x, y, width, height);

		p->restore();
		
		break;
	}
	case PE_IndicatorHeaderArrow: {
		QStyleOption arrow(*opt);
		arrow.state |= State_Enabled;
		proxy()->drawPrimitive((arrow.state & State_UpArrow) ? PE_IndicatorArrowUp : PE_IndicatorArrowDown, &arrow, p, widget);
		break;
	}
	case PE_IndicatorSpinUp:
	case PE_IndicatorSpinDown: {
		QStyleOption arrow(*opt);
	    arrow.rect.adjust(1,3,-3,-1);
		
		if (pe==PE_IndicatorSpinUp)
			proxy()->drawPrimitive(PE_IndicatorArrowUp, &arrow, p, widget);
		else
			proxy()->drawPrimitive(PE_IndicatorArrowDown, &arrow, p, widget);
		
		break;
	}

	// PROGRESS CHUNK
	// -------------------------------------------------------------------
	case PE_IndicatorProgressChunk: {
		drawGradientBox(p, opt, cdata, false, 0.92,1.66);
		break;
	}
		
	default: {
		QCommonStyle::drawPrimitive(pe, opt, p, widget);
		break;
	}
	}
}

void
BluecurveStyle::drawControl(ControlElement control, const QStyleOption *opt,
							QPainter *p, const QWidget *widget) const
{
	const qreal dpr = getDpr(p);
	const qreal inverseScale = qreal(1) / dpr;
	QRect r; // Drawing rectangle for elements which must not be scaled
	bool isScaled = false;
	if (!qFuzzyCompare(dpr, qreal(1))) {
		isScaled = true;
	    r = getScaledRect(opt->rect, dpr);
	} else {
		r = opt->rect;
	}
	
	const BluecurveColorData *cdata = lookupData(opt->palette);
	
	switch (control) {
	// CHECK/RADIO
	// -------------------------------------------------------------------
	case CE_CheckBox:
	case CE_RadioButton: {
		if (opt->state & State_MouseOver) // draw the highlight if hovered, and then pass on to QCommonStyle
			p->fillRect(opt->rect, opt->palette.midlight());
		QCommonStyle::drawControl(control, opt, p, widget);
		break;
	}

	// HEADER SECTION
	// -------------------------------------------------------------------		
	case CE_HeaderSection: {
        drawLightBevel(p, opt, &opt->palette.button(), true); 		
		break;
	}

	// TOOLBAR
    // -------------------------------------------------------------------	
	case CE_ToolBar: {		
		p->fillRect(opt->rect, opt->palette.window());

		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}
		
		p->setPen(cdata->bgShades[0]);
		p->drawLine(r.left(), r.top(), r.right(), r.top());
		p->setPen(cdata->bgShades[3]);
		p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());

		p->restore();
		
		break;
	}

	// SPLITTER
	// -------------------------------------------------------------------
	case CE_Splitter: {
		if (opt->state & State_MouseOver)
			p->fillRect(opt->rect, opt->palette.midlight());

		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}		

		if (opt->state & State_Horizontal) {
			int xx = r.left() + (r.width() - 4) / 2;
			int yy = r.top() + (r.height() - 24) / 2;

			for (int i = 0; i < 5; yy += 5, i++) {
				p->setPen(cdata->bgShades[5]);
				p->drawLine(xx, yy + 3, xx + 3, yy);
				p->setPen(Qt::white);
				p->drawLine(xx, yy + 4, xx + 3, yy + 1);
			}			
		} else {
		    int xx = r.left() + (r.width() - 24) / 2;
			int yy = r.top() + (r.height() - 4) / 2;

			for (int i = 0; i < 5; xx += 5, i++) {
				p->setPen(cdata->bgShades[5]);
				p->drawLine(xx + 3, yy,
							xx, yy + 3);
				p->setPen(Qt::white);
				p->drawLine(xx + 3, yy + 1,
							xx + 1, yy + 4);
			}
		}

		p->restore();

		break;
	}

	// SCROLLBAR
	// -------------------------------------------------------------------
	case CE_ScrollBarAddLine:
	case CE_ScrollBarSubLine: {
		const QStyleOptionSlider *scrollbar = qstyleoption_cast<const QStyleOptionSlider *>(opt);
		if (!scrollbar)
			return;

		// Compute state for the button (if the scrollbar is at the edge, it *must* be disabled
		bool isMin = (scrollbar->sliderValue <= scrollbar->minimum); // first check if the scrollbar is at min or max value
        bool isMax = (scrollbar->sliderValue >= scrollbar->maximum);
		bool isStart = scrollbar->upsideDown ? isMax : isMin;
        bool isEnd   = scrollbar->upsideDown ? isMin : isMax;
		bool disabled = ((control == CE_ScrollBarSubLine) && isStart) || ((control == CE_ScrollBarAddLine) && isEnd);

		// Palette used for scrollbar
		QPalette pal = scrollbar->palette;
		if (disabled)
			pal.setCurrentColorGroup(QPalette::Disabled);

		// state flags
		QStyle::State state = scrollbar->state;
		if (disabled && (scrollbar->state & State_Enabled)) {
			state &= ~State_Enabled;
			state &= ~State_Sunken;
			state &= ~State_MouseOver;
			state |= State_Raised;
		} else if (!(state & State_Sunken))
			state |= State_Raised;
		
		// Draw button background
		QStyleOptionButton btn;
		btn.rect = scrollbar->rect;
		btn.palette = pal;
		btn.state = state;
	    proxy()->drawPrimitive(PE_PanelButtonBevel, &btn, p, widget);

		PrimitiveElement pe;
		if ((control == CE_ScrollBarAddLine) && (opt->state & State_Horizontal))
			pe = PE_IndicatorArrowRight;
		else if (control == CE_ScrollBarAddLine)
			pe = PE_IndicatorArrowDown;
		else if (opt->state & State_Horizontal)
			pe = PE_IndicatorArrowLeft;
		else
			pe = PE_IndicatorArrowUp;

		// Draw arrow
		QStyleOption arrow;
		arrow.palette = pal;
		arrow.rect = scrollbar->rect.adjusted(3,3,-3,-3);
		arrow.state = state;
	    proxy()->drawPrimitive(pe, &arrow, p, widget);
	  
		break;
	}

	case CE_ScrollBarSubPage:
	case CE_ScrollBarAddPage: {
		p->fillRect(opt->rect, cdata->bgShades[3]);

		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}
		
		p->setPen(cdata->bgShades[5]);
		if (opt->state & State_Horizontal) {
			p->drawLine(r.left(), r.top(), r.right(), r.top());
			p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
		} else {
			p->drawLine(r.left(), r.top(), r.left(), r.bottom());
			p->drawLine(r.right(), r.top(), r.right(), r.bottom());
		}

		p->restore();
		
		break;
	}

	case CE_ScrollBarSlider: {
		int x1, y1;

		// Highlight on mouse over
		QStyleOption bevel(*opt);
	    bevel.state = (opt->state & ~State_Sunken) | State_Raised;
	    drawLightBevel(p, &bevel, (opt->state & State_MouseOver) ? &opt->palette.midlight() : &opt->palette.button(), true, true);

		if (opt->state & State_Horizontal && opt->rect.width() < 31)
			break;
		if (!(opt->state & State_Horizontal) && opt->rect.height() < 31)
			break;

		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}

		// Scrollbar diagonal handle
		p->setPen(cdata->btnShades[5]);
		if (opt->state & State_Horizontal) {
			x1 = (r.left() + r.right()) / 2 - 8;
			y1 = ((r.top() + r.bottom()) - 6) / 2;
			p->drawLine(x1 + 5, y1, x1, y1 + 5);
			p->drawLine(x1 + 5 + 5, y1,	x1 + 5, y1 + 5);
			p->drawLine(x1 + 5 + 5*2, y1, x1 + 5*2, y1 + 5);
		} else {
			x1 = ((r.left() + r.right()) - 6) / 2;
			y1 = (r.top() + r.bottom()) / 2 - 8;
			p->drawLine(x1 + 5, y1,	x1, y1 + 5);
			p->drawLine(x1 + 5, y1 + 5,	x1, y1 + 5 + 5);
			p->drawLine(x1 + 5, y1 + 5*2, x1, y1 + 5 + 5*2);
		}

		p->setPen(Qt::white);
		if (opt->state & State_Horizontal) {
			x1 = (r.left() + r.right()) / 2 - 8;
			y1 = ((r.top() + r.bottom()) - 6) / 2;
			p->drawLine(x1 + 5, y1+1, x1 + 1, y1 + 5);
			p->drawLine(x1 + 5 + 5, y1 + 1,	x1 + 1 + 5, y1 + 5);
			p->drawLine(x1 + 5 + 5*2, y1 + 1, x1 + 1 + 5*2, y1 + 5);
		} else {
			x1 = ((r.left() + r.right()) - 6) / 2;
			y1 = (r.top() + r.bottom()) / 2 - 8;
			p->drawLine(x1 + 5, y1 + 1,	x1 + 1, y1 + 5);
			p->drawLine(x1 + 5, y1 + 1 + 5,	x1 + 1, y1 + 5 + 5);
			p->drawLine(x1 + 5, y1 + 1 + 5*2, x1 + 1, y1 + 5 + 5*2);
		}

		p->restore();		
		break;
	}
		
	// BUTTON LABELS (adopted from QCommonStyle)
	// -------------------------------------------------------------------
	case CE_PushButtonLabel: {
		const QStyleOptionButton *button = qstyleoption_cast<const QStyleOptionButton *>(opt);
		if (!button)
			break;
		
		QRect textRect = button->rect;
		int tf = Qt::AlignVCenter | Qt::TextShowMnemonic;
		if (!proxy()->styleHint(SH_UnderlineShortcut, button, widget))
			tf |= Qt::TextHideMnemonic;

		if (button->features & QStyleOptionButton::HasMenu) {
			int indicatorSize = proxy()->pixelMetric(PM_MenuButtonIndicator, button, widget);
			if (button->direction == Qt::LeftToRight)
				textRect = textRect.adjusted(0, 0, -indicatorSize, 0);
			else
				textRect = textRect.adjusted(indicatorSize, 0, 0, 0);
		}

		if (!button->icon.isNull()) {
			QIcon::Mode mode = QIcon::Normal; // Always use normal mode, per GTK 2 behaviour
			QIcon::State state = (button->state & State_On) ? QIcon::On : QIcon::Off;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			QPixmap pixmap = button->icon.pixmap(button->iconSize, dpr, mode, state);
#else
			QPixmap pixmap = button->icon.pixmap(widget ? widget->window()->windowHandle() : nullptr, button->iconSize, mode, state);
#endif
			if (!(button->state & State_Enabled))
				pixmap = pixmap_saturate_and_pixelate(pixmap, 0.8, true);

			int pixmapWidth = pixmap.width() / pixmap.devicePixelRatio();
			int pixmapHeight = pixmap.height() / pixmap.devicePixelRatio();
			int labelWidth = pixmapWidth;
			int labelHeight = pixmapHeight;
			int iconSpacing = 4;//### 4 is currently hardcoded in QPushButton::sizeHint()
			if (!button->text.isEmpty()) {
				int textWidth = button->fontMetrics.boundingRect(opt->rect, tf, button->text).width();
				labelWidth += (textWidth + iconSpacing);
			}

			QRect iconRect = QRect(textRect.x() + (textRect.width() - labelWidth) / 2,
								   textRect.y() + (textRect.height() - labelHeight) / 2,
								   pixmapWidth, pixmapHeight);

			iconRect = visualRect(button->direction, textRect, iconRect);

			if (button->direction == Qt::RightToLeft)
				textRect.setRight(iconRect.left() - iconSpacing / 2);
			else
				textRect.setLeft(iconRect.left() + iconRect.width() + iconSpacing / 2);

			// qt_format_text reverses again when  painter->layoutDirection is also RightToLeft
			if (p->layoutDirection() == button->direction)
				tf |= Qt::AlignLeft;
			else
				tf |= Qt::AlignRight;

			if (button->state & (State_On | State_Sunken))
				iconRect.translate(proxy()->pixelMetric(PM_ButtonShiftHorizontal, opt, widget),
								   proxy()->pixelMetric(PM_ButtonShiftVertical, opt, widget));

			p->drawPixmap(iconRect, pixmap);
			
		} else {
			tf |= Qt::AlignHCenter;
		}

		proxy()->drawItemText(p, textRect, tf, button->palette, (button->state & State_Enabled),
					 button->text, QPalette::ButtonText);
		
		break;
	}
		
	case CE_ToolButtonLabel: {
		const QStyleOptionToolButton *toolbutton = qstyleoption_cast<const QStyleOptionToolButton *>(opt);
		if (!toolbutton)
			break;

		QRect rect = toolbutton->rect;
		int shiftX = 0;
		int shiftY = 0;
		if (toolbutton->state & (State_Sunken | State_On)) {
			shiftX = proxy()->pixelMetric(PM_ButtonShiftHorizontal, toolbutton, widget);
			shiftY = proxy()->pixelMetric(PM_ButtonShiftVertical, toolbutton, widget);
		}

		bool hasArrow = toolbutton->features & QStyleOptionToolButton::Arrow;
		if (((!hasArrow && toolbutton->icon.isNull()) && !toolbutton->text.isEmpty())
			|| toolbutton->toolButtonStyle == Qt::ToolButtonTextOnly) {
			int alignment = Qt::AlignCenter | Qt::TextShowMnemonic;
			if (!proxy()->styleHint(SH_UnderlineShortcut, opt, widget))
				alignment |= Qt::TextHideMnemonic;
			p->setFont(toolbutton->font);
			proxy()->drawItemText(p, rect, alignment, toolbutton->palette,
								  opt->state & State_Enabled, toolbutton->text,
								  QPalette::ButtonText);
		} else {

			auto drawToolArrow = [this, toolbutton, p, widget](const QRect &rect) {
				PrimitiveElement pe;
				switch (toolbutton->arrowType) {
				case Qt::LeftArrow:
					pe = PE_IndicatorArrowLeft;
					break;
				case Qt::RightArrow:
					pe = PE_IndicatorArrowRight;
					break;
				case Qt::UpArrow:
					pe = PE_IndicatorArrowUp;
					break;
				case Qt::DownArrow:
					pe = PE_IndicatorArrowDown;
					break;
				default:
					return;
				}
				QStyleOption arrowOpt(*toolbutton);
				arrowOpt.rect = rect;
				proxy()->drawPrimitive(pe, &arrowOpt, p, widget);				
			};
			
			QPixmap pm;
			QSize pmSize = toolbutton->iconSize;
			if (!toolbutton->icon.isNull()) {
				QIcon::State state = toolbutton->state & State_On ? QIcon::On : QIcon::Off;
				QIcon::Mode mode = QIcon::Normal;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			    pm = toolbutton->icon.pixmap(toolbutton->rect.size().boundedTo(toolbutton->iconSize),
											 dpr, mode, state);
#else
				pm = toolbutton->icon.pixmap(widget ? widget->window()->windowHandle() : nullptr,
											 toolbutton->rect.size().boundedTo(toolbutton->iconSize),
											 mode, state);
#endif
				if (!(toolbutton->state & State_Enabled))
					pm = pixmap_saturate_and_pixelate(pm, 0.8, true);

				pmSize = pm.size() / pm.devicePixelRatio();
			}

			if (toolbutton->toolButtonStyle != Qt::ToolButtonIconOnly) {
				p->setFont(toolbutton->font);
				QRect pr = rect,
                    tr = rect;
				int alignment = Qt::TextShowMnemonic;
				if (!proxy()->styleHint(SH_UnderlineShortcut, opt, widget))
					alignment |= Qt::TextHideMnemonic;

				if (toolbutton->toolButtonStyle == Qt::ToolButtonTextUnderIcon) {
					pr.setHeight(pmSize.height() + 4); //### 4 is currently hardcoded in QToolButton::sizeHint()
					tr.adjust(0, pr.height() - 1, 0, -1);
					pr.translate(shiftX, shiftY);
					if (!hasArrow) {
					    proxy()->drawItemPixmap(p, pr, Qt::AlignCenter, pm);
					} else {
						drawToolArrow(pr);
					}
					alignment |= Qt::AlignCenter;					
				} else {
					pr.setWidth(pmSize.width() + 4); //### 4 is currently hardcoded in QToolButton::sizeHint()
					tr.adjust(pr.width(), 0, 0, 0);
					pr.translate(shiftX, shiftY);
					if (!hasArrow) {
						proxy()->drawItemPixmap(p, QStyle::visualRect(opt->direction, rect, pr), Qt::AlignCenter, pm);
					} else {
						drawToolArrow(pr);
					}
					alignment |= Qt::AlignLeft | Qt::AlignVCenter;
				}
				tr.translate(shiftX, shiftY);
				proxy()->drawItemText(p, QStyle::visualRect(opt->direction, rect, tr), alignment, toolbutton->palette,
									  toolbutton->state & State_Enabled, toolbutton->text,
									  QPalette::ButtonText);
			} else {
				rect.translate(shiftX, shiftY);
				if (hasArrow) {
					drawToolArrow(rect);
				} else {
				    proxy()->drawItemPixmap(p, rect, Qt::AlignCenter, pm);
				}
			}
		}
		
		break;
	}

	// TABBAR TABS
	// -------------------------------------------------------------------
	case CE_TabBarTabShape: {
		const QStyleOptionTab *tb = qstyleoption_cast<const QStyleOptionTab *>(opt);
		if (!tb)
			break;
		QTabBar::Shape tbs = tb->shape;
		bool selected = tb->state & State_Selected;
		bool first = tb->position == QStyleOptionTab::Beginning || tb->position == QStyleOptionTab::OnlyOneTab;
		bool last = tb->position == QStyleOptionTab::End || tb->position == QStyleOptionTab::OnlyOneTab;
		const int baseOverlap = proxy()->pixelMetric(PM_TabBarBaseOverlap);
		QRect tr, fr;	

		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}

		switch (tbs) {
		case QTabBar::RoundedNorth:
		case QTabBar::TriangularNorth: {
			// Rect calculations
			QRect tabRect = tb->rect.adjusted( (selected && !first) ? -1 : 0, !selected ? 1 : 0, !last ? 1 : 0, 0);
			if (isScaled)
				tr = getScaledRect(tabRect, dpr);
			else
				tr = tabRect;
			
			// Tab border
			p->setPen(cdata->bgShades[6]);
			p->drawLine(tr.left(), tr.top(), tr.right(), tr.top()); // top
			p->drawLine(tr.left(), tr.top(), tr.left(), tr.bottom() - baseOverlap); // left
			p->drawLine(tr.right(), tr.top(), tr.right(), tr.bottom() - baseOverlap); // right

			// Inner shading
			p->setPen(Qt::white);
			p->drawLine(tr.left() + 1, tr.top() + 1, tr.right() - 1, tr.top() + 1); // top
			if (selected || first)
				p->drawLine(tr.left() + 1, tr.top() + 1, tr.left() + 1, tr.bottom() - (selected ? 0 : baseOverlap)); // left
			p->setPen(cdata->bgShades[2]);
			p->drawLine(tr.right() - 1, tr.top() + 1, tr.right() -1, tr.bottom() - (selected ? 0 : baseOverlap)); // right

			// Fill rectangle
			fr = tr.adjusted((selected || first) ? 2 : 1, 2, -2, selected ? 0 : -2);
			
			break;
		}

		case QTabBar::RoundedSouth:
		case QTabBar::TriangularSouth: {
			// Rect calculations
			QRect tabRect = tb->rect.adjusted( (selected && !first) ? -1 : 0, 0, !last ? 1 : 0, !selected ? -1 : 0);
			if (isScaled)
				tr = getScaledRect(tabRect, dpr);
			else
				tr = tabRect;
			
			// Tab border
			p->setPen(cdata->bgShades[6]);
		    p->drawLine(tr.left(), tr.bottom(), tr.right(), tr.bottom()); // bottom
			p->drawLine(tr.left(), tr.top() + baseOverlap, tr.left(), tr.bottom()); // left
			p->drawLine(tr.right(), tr.top() + baseOverlap, tr.right(), tr.bottom()); // right

			// Inner shading
			p->setPen(cdata->bgShades[2]);
			p->drawLine(tr.left() + 1, tr.bottom() - 1, tr.right() - 1, tr.bottom() - 1); // bottom
			p->drawLine(tr.right() - 1, tr.top() + (selected ? 0 : baseOverlap), tr.right() - 1, tr.bottom() - 1); // right
			if (selected || first) {
				p->setPen(Qt::white);
				p->drawLine(tr.left() + 1, tr.top() + (selected ? 0 : baseOverlap), tr.left() + 1, tr.bottom() - 1); // left
			}

			// Fill rectangle
			fr = tr.adjusted((selected || first) ? 2 : 1, selected ? 0 : 2, -2, -2);
			
			break;
		}

		case QTabBar::RoundedWest:
		case QTabBar::TriangularWest: {
			// Rect calculations
			QRect tabRect = tb->rect.adjusted(!selected ? 1 : 0, (selected && !first) ? -1 : 0, 0, !last ? 1 : 0);
			if (isScaled)
				tr = getScaledRect(tabRect, dpr);
			else
				tr = tabRect;
			
			// Tab border
			p->setPen(cdata->bgShades[6]);
			p->drawLine(tr.left(), tr.top(), tr.left(), tr.bottom()); // left
			p->drawLine(tr.left(), tr.top(), tr.right() - baseOverlap, tr.top()); // top
			p->drawLine(tr.left(), tr.bottom(), tr.right() - baseOverlap, tr.bottom()); // bottom

			// Inner shading
			p->setPen(Qt::white);
			p->drawLine(tr.left() + 1, tr.top() + 1, tr.left() + 1, tr.bottom() - 1); // left
			if (selected || first)
				p->drawLine(tr.left() + 1, tr.top() + 1, tr.right() - (selected ? 0 : baseOverlap), tr.top() + 1); // top
			p->setPen(cdata->bgShades[2]);
			p->drawLine(tr.left() + 1, tr.bottom() - 1, tr.right() - (selected ? 0 : baseOverlap), tr.bottom() - 1); // bottom

			// Fill rectangle
			fr = tr.adjusted(2, (selected || first) ? 2 : 1, selected ? 0 : -2, -2);
			
			break;
		}

		case QTabBar::RoundedEast:
		case QTabBar::TriangularEast: {
			// Rect calculations
			QRect tabRect = tb->rect.adjusted(0, (selected && !first) ? -1 : 0, !selected ? -1 : 0, !last ? 1 : 0);
			if (isScaled)
				tr = getScaledRect(tabRect, dpr);
			else
				tr = tabRect;
			
			// Tab border
			p->setPen(cdata->bgShades[6]);
			p->drawLine(tr.right(), tr.top(), tr.right(), tr.bottom()); // right
			p->drawLine(tr.left() + baseOverlap, tr.top(), tr.right(), tr.top()); // top
			p->drawLine(tr.left() + baseOverlap, tr.bottom(), tr.right(), tr.bottom()); // bottom

			// Inner shading
			p->setPen(cdata->bgShades[2]);
			p->drawLine(tr.right() - 1, tr.top() + 1, tr.right() - 1, tr.bottom() - 1); // right
			p->drawLine(tr.left() + (selected ? 0 : baseOverlap), tr.bottom() - 1, tr.right() - 1, tr.bottom() - 1); // bottom
			if (selected || first) {
				p->setPen(Qt::white);
				p->drawLine(tr.left() + (selected ? 0 : baseOverlap), tr.top() + 1, tr.right() - 1, tr.top() + 1); // top
			}

			// Fill rectangle
			fr = tr.adjusted(selected ? 0 : 2, (selected || first) ? 2 : 1, -2, -2);
			
			break;
		}

		default: {
			// Should never reach here, but we leave it in case Qt introduces new tab shapes
			break;
		}
			
		}

		// Apply the fill
		if (isScaled)
			p->translate(-0.5,-0.5);
		p->fillRect(fr, selected ? tb->palette.window() : tb->palette.mid());
		
		p->restore();		
		break;
	}


	case CE_TabBarTabLabel: {
		const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab *>(opt);
		if (!tab)
			break;

		QRect tr = tab->rect;
		bool verticalTabs = tab->shape == QTabBar::RoundedEast
			|| tab->shape == QTabBar::RoundedWest
			|| tab->shape == QTabBar::TriangularEast
			|| tab->shape == QTabBar::TriangularWest;

		int alignment = Qt::AlignCenter | Qt::TextShowMnemonic;
		if (!proxy()->styleHint(SH_UnderlineShortcut, opt, widget))
			alignment |= Qt::TextHideMnemonic;

		if (verticalTabs) {
			p->save();
			int newX, newY, newRot;
			if (tab->shape == QTabBar::RoundedEast || tab->shape == QTabBar::TriangularEast) {
				newX = tr.width() + tr.x();
				newY = tr.y();
				newRot = 90;
			} else {
				newX = tr.x();
				newY = tr.y() + tr.height();
				newRot = -90;
			}
			QTransform m = QTransform::fromTranslate(newX, newY);
			m.rotate(newRot);
			p->setTransform(m, true);
		}
		QRect iconRect;
		tabLayout(tab, widget, &tr, &iconRect);
		tr = proxy()->subElementRect(SE_TabBarTabText, opt, widget); //we compute tr twice because the style may override subElementRect

		if (!tab->icon.isNull()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)			
			QPixmap tabIcon = tab->icon.pixmap(tab->iconSize, dpr, QIcon::Normal,
											   (tab->state & State_Selected) ? QIcon::On : QIcon::Off);
#else
			QPixmap tabIcon = tab->icon.pixmap(widget ? widget->window()->windowHandle() : nullptr, tab->iconSize, QIcon::Normal,
											   (tab->state & State_Selected) ? QIcon::On : QIcon::Off);
#endif
			if (!(tab->state & State_Enabled))
				tabIcon = pixmap_saturate_and_pixelate(tabIcon, 0.8, true);			
			p->drawPixmap(iconRect.x(), iconRect.y(), tabIcon);
		}

		proxy()->drawItemText(p, tr, alignment, tab->palette, tab->state & State_Enabled, tab->text,
							  widget ? widget->foregroundRole() : QPalette::ButtonText);
		if (verticalTabs)
			p->restore();

		if (tab->state & State_HasFocus) {
			const int OFFSET = 1 + pixelMetric(PM_DefaultFrameWidth);

			int x1, x2;
			x1 = tab->rect.left();
			x2 = tab->rect.right() - 1;

			QStyleOptionFocusRect fropt;
			fropt.QStyleOption::operator=(*tab);
			fropt.rect.setRect(x1 + 1 + OFFSET, tab->rect.y() + OFFSET,
							   x2 - x1 - 2*OFFSET, tab->rect.height() - 2*OFFSET);
			proxy()->drawPrimitive(PE_FrameFocusRect, &fropt, p, widget);
		}
		
		break;
	}

	// TOOLBOX TABS
	// -------------------------------------------------------------------
	case CE_ToolBoxTabShape: {
		// Use the same background as a regular tab
		bool selected = opt->state & State_Selected;
		
		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}
			
		// Tab border
		p->setPen(cdata->bgShades[6]);
		p->drawLine(r.left(), r.top(), r.right(), r.top()); // top
		p->drawLine(r.left(), r.top(), r.left(), r.bottom()); // left
		p->drawLine(r.right(), r.top(), r.right(), r.bottom()); // right

		// Inner shading
		p->setPen(Qt::white);
		p->drawLine(r.left() + 1, r.top() + 1, r.right() - 1, r.top() + 1); // top
		p->drawLine(r.left() + 1, r.top() + 1, r.left() + 1, r.bottom()); // left
		p->setPen(cdata->bgShades[2]);
		p->drawLine(r.right() - 1, r.top() + 1, r.right() -1, r.bottom()); // right

		// Fill rectangle
		QRect fr = r.adjusted(2, 2, -2, 0);
		if (isScaled)
			p->translate(-0.5,-0.5);
		p->fillRect(fr, selected ? opt->palette.window() : opt->palette.mid());
		p->restore();
		
		break;
	}

	case CE_ToolBoxTabLabel: {
		const QStyleOptionToolBox *tb = qstyleoption_cast<const QStyleOptionToolBox *>(opt);
		if (!tb)
			break;

		bool enabled = tb->state & State_Enabled;
		bool selected = tb->state & State_Selected;
		int iconExtent = proxy()->pixelMetric(QStyle::PM_SmallIconSize, tb, widget);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		QPixmap pm = tb->icon.pixmap(QSize(iconExtent,iconExtent), dpr, QIcon::Normal, selected ? QIcon::On : QIcon::Off);
#else
		QPixmap pm = tb->icon.pixmap(widget ? widget->window()->windowHandle() : nullptr,
									 QSize(iconExtent,iconExtent), QIcon::Normal, selected ? QIcon::On : QIcon::Off);
#endif
		if (!enabled)
			pm = pixmap_saturate_and_pixelate(pm, DISABLED_ICON_SATURATION, true);

		QRect cr = proxy()->subElementRect(QStyle::SE_ToolBoxTabContents, tb, widget);
		QRect tr, ir;
		int ih = 0;
		if (pm.isNull()) {
			tr = cr;
			tr.adjust(4, 0, -8, 0);
		} else {
			int iw = pm.width() / pm.devicePixelRatio() + 4;
			ih = pm.height()/ pm.devicePixelRatio();
			ir = QRect(cr.left() + 4, cr.top(), iw + 2, ih);
			tr = QRect(ir.right(), cr.top(), cr.width() - ir.right() - 4, cr.height());
		}

		QString txt = tb->fontMetrics.elidedText(tb->text, Qt::ElideRight, tr.width());
		
		if (ih)
			p->drawPixmap(ir.left(), (tb->rect.height() - ih) / 2, pm);

		int alignment = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic;
		if (!proxy()->styleHint(QStyle::SH_UnderlineShortcut, tb, widget))
			alignment |= Qt::TextHideMnemonic;
		proxy()->drawItemText(p, tr, alignment, tb->palette, enabled, txt, QPalette::ButtonText);

		if (!txt.isEmpty() && opt->state & State_HasFocus) {
			QStyleOptionFocusRect opt;
			opt.rect = tr;
			opt.palette = tb->palette;
			opt.state = QStyle::State_None;
			proxy()->drawPrimitive(QStyle::PE_FrameFocusRect, &opt, p, widget);
		}
		break;		
	}

	// MENU/MENUBAR ITEMS
	// -------------------------------------------------------------------
	case CE_MenuItem: {
		const QStyleOptionMenuItem *menuitem = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
		if (!menuitem)
			break;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		const int tab = menuitem->reservedShortcutWidth;
#else
		const int tab = menuitem->tabWidth;
#endif
		const int checkcol = qMax<int>(menuitem->maxIconWidth, 22);
		const int itemHMargin = 4;
		const int arrowWidth = 8; // arrow size taken from GTK 2 theme
		const int arrowHeight = 9;
		
		bool enabled = menuitem->state & State_Enabled;
		bool checked = menuitem->checkType != QStyleOptionMenuItem::NotCheckable
			? menuitem->checked : false;
		bool active = menuitem->state & State_Selected;
		bool reverse = QGuiApplication::isRightToLeft();

		// Separator
		if ( menuitem->menuItemType == QStyleOptionMenuItem::Separator ) {
			p->fillRect(menuitem->rect, menuitem->palette.window());

			p->save();
			if (isScaled) {
				p->scale(inverseScale, inverseScale);
				p->translate(0.5, 0.5);
			}
			
			p->setPen(cdata->bgShades[2]);
			p->drawLine(r.left() + 6, r.top() + 4, r.right() - 6, r.top() + 4);
			p->setPen(Qt::white);
			p->drawLine(r.left() + 6,  r.top() + 5, r.right() - 6, r.top() + 5);

			p->restore();
			break;
		}

		// Menu background
		if (enabled && active)
			drawGradientBox(p, opt, cdata, false, 0.9, 1.2);
		else 
			p->fillRect(opt->rect, opt->palette.button());

		// compute rects
		int x,y,w,h;
		menuitem->rect.getRect(&x,&y,&w,&h);
		QRect cr(x, y, checkcol, h); // Check mark rect
		QRect sr(x + w - arrowWidth - itemHMargin, y + (h - arrowHeight)/2, arrowWidth, arrowHeight); // Sub menu arrow indicator rect (NB: we must always reserve this width, since even menus with submenus can have accelerator texts)
		QRect tr(sr.left() - tab - itemHMargin, y, tab, h); // tab/accelerator rect
		QRect ir(cr.right() + itemHMargin, y, tr.left() - cr.right() - 2 * itemHMargin - x, h); // main text rect
		if ( reverse ) {
			cr = visualRect( opt->direction, menuitem->rect, cr );
			sr = visualRect( opt->direction, menuitem->rect, sr );
			tr = visualRect( opt->direction, menuitem->rect, tr );
			ir = visualRect( opt->direction, menuitem->rect, tr );
		}

		// If the menu item has an icon and is checkable, draw a sunken shaded rect around the icon if checked
		// If the menu item does not have an icon and is checkable, draw a standard checkmark
		if (!menuitem->icon.isNull()) {
			if (checked) { // Draw the sunken background
				QStyleOption checkFrame(*menuitem);
				checkFrame.rect = cr;
				checkFrame.state |= State_Sunken;
				drawLightBevel(p, &checkFrame, active ? &menuitem->palette.midlight() : &menuitem->palette.mid(), true, true);
			}
			
			const auto size = proxy()->pixelMetric(PM_SmallIconSize, opt, widget);
			const auto state = checked ? QIcon::On : QIcon::Off;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			QPixmap pixmap = menuitem->icon.pixmap(QSize(size,size), dpr, QIcon::Normal, state);
#else
			QPixmap pixmap = menuitem->icon.pixmap(widget ? widget->window()->windowHandle() : nullptr,
												   QSize(size,size), QIcon::Normal, state);
#endif
			if (!enabled)
				pixmap = pixmap_saturate_and_pixelate(pixmap, DISABLED_ICON_SATURATION, true);
			QRect pmr(QPoint(0, 0), pixmap.size() / pixmap.devicePixelRatio());
			pmr.moveCenter(cr.center());
			p->setPen(menuitem->palette.text().color());
			p->drawPixmap(pmr.topLeft(), pixmap);
		} else if (checked) {
			QStyleOption check(*menuitem);
			check.rect = cr;
			proxy()->drawPrimitive(PE_IndicatorMenuCheckMark, &check, p, widget);
		}

		// Draw the text
		QStringView s(menuitem->text);
		if (!s.isEmpty()) {
			// Set up text
			qsizetype t = s.indexOf(u'\t');
			int tf = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
			if (!proxy()->styleHint(SH_UnderlineShortcut, menuitem, widget))
				tf |= Qt::TextHideMnemonic;
			QPalette::ColorRole textRole = active ? QPalette::HighlightedText : QPalette::ButtonText;

			// draw accelerator/tab-text
			if (t >= 0) {
				const QString textToDraw = s.mid(t + 1).toString();
				int alignFlag = tf | ( reverse ? Qt::AlignLeft : Qt::AlignRight );
				proxy()->drawItemText(p, tr, alignFlag, menuitem->palette, enabled,
									  textToDraw, textRole);
			}

			// Draw main item text
			const QString textToDraw = s.left(t).toString();
			int alignFlag = tf | ( reverse ? Qt::AlignRight : Qt::AlignLeft );
			proxy()->drawItemText(p, ir, alignFlag, menuitem->palette, enabled,
								  textToDraw, textRole);
		}

		if (menuitem->menuItemType == QStyleOptionMenuItem::SubMenu) {
			QStyleOption arrow(*menuitem);
			arrow.rect = sr;
			proxy()->drawPrimitive((reverse ? PE_IndicatorArrowLeft : PE_IndicatorArrowRight), &arrow, p, widget);
		}

		break;
	}

	case CE_MenuBarEmptyArea: {
		p->fillRect(r, opt->palette.button());
		break;
	}

	case CE_MenuBarItem: {
		const QStyleOptionMenuItem *menuitem = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
		if (!menuitem)
			break;

		// Menu item background
		if ((opt->state & State_Enabled) && (opt->state & State_Sunken))
			drawGradientBox(p, opt, cdata, false, 0.9, 1.2);
		else
			p->fillRect(menuitem->rect, menuitem->palette.button());

		// Draw the text
		int tf = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
		if (!proxy()->styleHint(SH_UnderlineShortcut, menuitem, widget))
			tf |= Qt::TextHideMnemonic;
		
		proxy()->drawItemText(p, menuitem->rect, tf, menuitem->palette, menuitem->state & State_Enabled,
							  menuitem->text, (menuitem->state & State_Sunken) ? QPalette::HighlightedText : QPalette::ButtonText);
		
		break;
	}

	// PROGRESS BAR
	// -------------------------------------------------------------------
	case CE_ProgressBarGroove: {
		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}
		
		p->setBrush(cdata->bgShades[3]);
		p->setPen(cdata->bgShades[5]);
		p->drawRect(r.adjusted(0,0,-1,-1));

		p->restore();
		break;
	}

	case CE_ProgressBarContents: {
		const QStyleOptionProgressBar *progressbar = qstyleoption_cast<const QStyleOptionProgressBar *>(opt);
		if (!progressbar)
			return;
	    bool reverse = QGuiApplication::isRightToLeft();

		QRect pr;

		if ((progressbar->minimum == 0) && (progressbar->maximum == 0)) {
			int w, remains;

			// draw busy indicator

			w = std::min(25, opt->rect.width()/2);
			w = std::max(w, 1);

			remains = opt->rect.width() - w;
			remains = std::max(remains, 1);

			int x = progressbar->progress % (remains * 2);
			if (x > remains)
				x = 2 * remains - x;

			x = reverse ? opt->rect.right() - x - w : x + opt->rect.left();
			pr.setRect (x, opt->rect.top(), w, opt->rect.height());
		} else {
			int pos = progressbar->progress;
			int total = (progressbar->maximum - progressbar->minimum) ?
				(progressbar->maximum - progressbar->minimum) : 1;
			int w = (int)(((double)pos*opt->rect.width())/total);

			if (reverse)
				pr.setRect (opt->rect.right() - w, opt->rect.top(), w, opt->rect.height());
			else
				pr.setRect (opt->rect.left(), opt->rect.top(), w, opt->rect.height());
		}
		QStyleOption optCopy(*opt);
		optCopy.rect = pr;
		
		drawGradientBox(p, &optCopy, cdata, false, 0.92, 1.66);
		
		break;
	}	

	// RESIZE GRIP (adopted from Bluecurve GTK+2.0 theme engine)
	// -------------------------------------------------------------------	
	case CE_SizeGrip: {
		const QStyleOptionSizeGrip *grip = qstyleoption_cast<const QStyleOptionSizeGrip *>(opt);
		if (!grip)
			break;

		p->save();
		if (isScaled) {
			p->scale(inverseScale, inverseScale);
			p->translate(0.5, 0.5);
		}

		int x = r.x();
		int y = r.y();
		int width = r.width();
		int height = r.height();

		switch (grip->corner) {
		case Qt::TopLeftCorner: {
			x += 2;
			y += 2;
			width -= 2;
			height -= 2;
			
			int xi, yi;

			xi = x + width;
			yi = y + height;

			while (xi > x + 3) {
				p->setPen(cdata->bgShades[5]);
				p->drawLine(xi, y, x, yi);

				--xi;
				--yi;

				p->setPen(Qt::white);
				p->drawLine(xi, y, x, yi);

				xi -= 3;
				yi -= 3;	    
			}
			
			break;
		}

		case Qt::TopRightCorner: {
			y += 2;
			width -= 2;
			height -= 2;
			
			int xi, yi;

			xi = x;
			yi = y + height;

			while (xi < (x + width - 3)) {
				p->setPen(Qt::white);
				p->drawLine(xi, y, x + width, yi);

				++xi;
				--yi;

				p->setPen(cdata->bgShades[5]);
				p->drawLine(xi, y, x + width, yi);

				xi += 3;
				yi -= 3;
			}
			
			break;
		}

		case Qt::BottomLeftCorner: {
			x += 2;
			width -= 2;
			height -= 2;
			
			int xi, yi;

			xi = x + width;
			yi = y;

			while (xi > x + 3) {
				p->setPen(cdata->bgShades[5]);
				p->drawLine(x, yi, xi, y + height);

				--xi;
				++yi;

				p->setPen(Qt::white);
				p->drawLine(x, yi, xi, y + height);

				xi -= 3;
				yi += 3;
			}
	   
			break;
		}
			
		case Qt::BottomRightCorner: {
			width -= 2;
			height -= 2;
			
			int xi, yi;

			xi = x;
			yi = y;

			while (xi < (x + width - 3)) {
				p->setPen(Qt::white);
				p->drawLine(xi, y + height, x + width, yi);

				++xi;
				++yi;

				p->setPen(cdata->bgShades[5]);
				p->drawLine(xi, y + height, x + width, yi);
            
				xi += 3;
				yi += 3;
			}
			
			break;
		}
			
		default: { // should never reach this point...
			break;
		}
		}

		p->restore();
		break;
	}
		
	default: {
		QCommonStyle::drawControl(control, opt, p, widget);
		break;
	}
	}
	
}

QRect
BluecurveStyle::subElementRect(SubElement element, const QStyleOption *opt,
							   const QWidget *widget) const
{
	QRect rect = QCommonStyle::subElementRect(element, opt, widget);

	switch (element) {
	case SE_CheckBoxIndicator:
	case SE_RadioButtonIndicator: {
		rect.translate(2,0);
		break;
	}

	case SE_ComboBoxFocusRect: {
		bool reverse = (opt->direction == Qt::RightToLeft);
		rect.adjust(reverse ? 3 : 0, 0, reverse ? 0 : -3, 0);		
		break;		
	}
		
	default: {
		break;
	}
	}

	return rect;
}

void
BluecurveStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *opt,
								   QPainter *p, const QWidget *widget) const
{
	const qreal dpr = getDpr(p);
	const qreal inverseScale = qreal(1) / dpr;
	bool isScaled = qFuzzyCompare(dpr, qreal(1)) ? false : true;
	
	const BluecurveColorData *cdata = lookupData(opt->palette);
	
	switch (control) {
	// COMBOBOX
	// -------------------------------------------------------------------
	case CC_ComboBox: {
		const QStyleOptionComboBox *combobox = qstyleoption_cast<const QStyleOptionComboBox *>(opt);
		if (!combobox)
			break;

		QRect frame, arrow, field;
		frame = proxy()->subControlRect(CC_ComboBox, opt, SC_ComboBoxFrame, widget);
		arrow = proxy()->subControlRect(CC_ComboBox, opt, SC_ComboBoxArrow, widget);
		field = proxy()->subControlRect(CC_ComboBox, opt, SC_ComboBoxEditField, widget);

		// Combobox frame (same as standard button)
		if (combobox->subControls & SC_ComboBoxFrame) {
			QStyleOptionButton button;
			button.QStyleOption::operator=(*combobox);
			button.rect = frame;
			proxy()->drawPrimitive(PE_PanelButtonBevel, &button, p, widget);
		}

		// Combobox arrow
		if (combobox->subControls & SC_ComboBoxArrow) {
			// Indicator arrow
			QStyleOption arrowOpt(*combobox);
		    arrowOpt.rect = QRect(0,0,9,8);
			arrowOpt.rect.moveCenter(arrow.center());
			arrowOpt.rect.translate(0,-2);
			drawPrimitive(PE_IndicatorArrowDown, &arrowOpt, p);

			// Shadow underneath the arrow
			p->fillRect(arrowOpt.rect.x() + 2,
						arrowOpt.rect.bottom() + 1,
						5, 2, cdata->btnShades[3]);
		}

		// Separator
		if ((combobox->subControls & SC_ComboBoxEditField) && (combobox->subControls & SC_ComboBoxArrow)) {
		   bool reverse = (combobox->direction == Qt::RightToLeft);
		   bool sunken = (combobox->state & (State_On | State_Sunken));
		   QRect r = getScaledRect(combobox->rect, dpr);

		   p->save();
		   if (isScaled) {
			   arrow = getScaledRect(arrow, dpr);
			   p->scale(inverseScale, inverseScale);
			   p->translate(0.5, 0.5);
		   }

		   if (reverse) {
			   p->setPen(sunken ? Qt::white : cdata->btnShades[2]);
			   p->drawLine(arrow.right() + 1, r.top() + 1 + (sunken ? 0 : 1), arrow.right() + 1, r.bottom() - 1);
			   p->setPen(cdata->btnShades[3]);
			   p->drawLine(arrow.right() + 2, r.top() + 1, arrow.right() + 2, r.bottom() - 1);
		   } else {
			   p->setPen(sunken ? cdata->btnShades[2] : Qt::white);
			   p->drawLine(arrow.left() - 1, r.top() + 1, arrow.left() - 1, r.bottom() - 1 - (sunken ? 1 : 0));
			   p->setPen(cdata->btnShades[3]);
			   p->drawLine(arrow.left() - 2, r.top() + 1, arrow.left() - 2, r.bottom() - 1);
		   }		   
		   
		   p->restore();
		}
		
		// Focus rect
		if ((combobox->subControls & SC_ComboBoxEditField) && (combobox->state & State_HasFocus) && !combobox->editable) {
			QStyleOptionFocusRect focus;
			focus.QStyleOption::operator=(*combobox);
			focus.rect = subElementRect(SE_ComboBoxFocusRect, combobox, widget);
			focus.state |= State_FocusAtBorder;
			proxy()->drawPrimitive(PE_FrameFocusRect, &focus, p, widget);
		}		
		
		break;
	}

	// SPINBOX
	// -------------------------------------------------------------------		
	case CC_SpinBox: {
		const QStyleOptionSpinBox *spinbox = qstyleoption_cast<const QStyleOptionSpinBox *>(opt);
		if (!spinbox)
			break;

		// Draw spin box text area
		if (spinbox->frame && (spinbox->subControls & SC_SpinBoxFrame)) {
			QStyleOption frame(*spinbox);
			frame.rect = proxy()->subControlRect(CC_SpinBox, spinbox, SC_SpinBoxFrame, widget);
			proxy()->drawPrimitive(PE_PanelLineEdit, &frame, p, widget);
		}

		// Up button
		if (spinbox->subControls & SC_SpinBoxUp) {
			QStyleOption spinBtn(*spinbox);
			spinBtn.rect = proxy()->subControlRect(CC_SpinBox, spinbox, SC_SpinBoxUp, widget);

			// Configure state/palette/fill for the spin button
			const QBrush *fill;
			QPalette pal = spinbox->palette;
			if (!(spinbox->stepEnabled & QAbstractSpinBox::StepUpEnabled)) { // If the spin button is disabled, apply disabled state/palette
				pal.setCurrentColorGroup(QPalette::Disabled);
				spinBtn.state &= ~State_Enabled;
			}
			spinBtn.palette = pal;			
			if (spinbox->activeSubControls == SC_SpinBoxUp && (spinbox->state & State_Sunken)) { // Set sunken/raised states as needed
				spinBtn.state |= State_On;
				spinBtn.state |= State_Sunken;
				fill = &opt->palette.mid();
			} else {
				spinBtn.state |= State_Raised;
				spinBtn.state &= ~State_Sunken;
				fill = &opt->palette.button();
			}
			if ((spinbox->state & State_MouseOver) && (spinbox->state & State_Enabled)) { // Check if the button is being hovered
				QPoint mousePos = widget ? widget->mapFromGlobal(QCursor::pos()) : QPoint();
				SubControl sc = QCommonStyle::hitTestComplexControl(CC_SpinBox, spinbox, mousePos, widget);
				if (sc == SC_SpinBoxUp) {
					spinBtn.state |= State_MouseOver;
					fill = &opt->palette.midlight();
				}
				else
					spinBtn.state &= ~State_MouseOver;
			}

			// Draw the button
			drawLightBevel(p, &spinBtn, fill, true);

			// Draw the indicator
			spinBtn.rect.adjust(1, 1, -1, -1);
			PrimitiveElement pe = (spinbox->buttonSymbols == QAbstractSpinBox::PlusMinus ? PE_IndicatorSpinPlus
								   : PE_IndicatorSpinUp);
			proxy()->drawPrimitive(pe, &spinBtn, p, widget);
		}

		// Down button
		if (spinbox->subControls & SC_SpinBoxDown) {
			QStyleOption spinBtn(*spinbox);
			spinBtn.rect = proxy()->subControlRect(CC_SpinBox, spinbox, SC_SpinBoxDown, widget);

			// Configure state/palette/fill for the spin button
			const QBrush *fill;
			QPalette pal = spinbox->palette;
			if (!(spinbox->stepEnabled & QAbstractSpinBox::StepDownEnabled)) { // If the spin button is disabled, apply disabled state/palette
				pal.setCurrentColorGroup(QPalette::Disabled);
				spinBtn.state &= ~State_Enabled;
			}
			spinBtn.palette = pal;			
			if (spinbox->activeSubControls == SC_SpinBoxDown && (spinbox->state & State_Sunken)) { // Set sunken/raised states as needed
				spinBtn.state |= State_On;
				spinBtn.state |= State_Sunken;
				fill = &opt->palette.mid();
			} else {
				spinBtn.state |= State_Raised;
				spinBtn.state &= ~State_Sunken;
				fill = &opt->palette.button();
			}
			if ((spinbox->state & State_MouseOver) && (spinbox->state & State_Enabled)) { // Check if the button is being hovered
				QPoint mousePos = widget ? widget->mapFromGlobal(QCursor::pos()) : QPoint();
				SubControl sc = QCommonStyle::hitTestComplexControl(CC_SpinBox, spinbox, mousePos, widget);
				if (sc == SC_SpinBoxDown) {
					spinBtn.state |= State_MouseOver;
					fill = &opt->palette.midlight();
				}
				else
					spinBtn.state &= ~State_MouseOver;
			}

			// Draw the button
			drawLightBevel(p, &spinBtn, fill, true);

			// Draw the indicator
			spinBtn.rect.adjust(1, 1, -1, -1);
			PrimitiveElement pe = (spinbox->buttonSymbols == QAbstractSpinBox::PlusMinus ? PE_IndicatorSpinMinus
								   : PE_IndicatorSpinDown);
			proxy()->drawPrimitive(pe, &spinBtn, p, widget);
		}
		
		break;
	}

	// SLIDER
	// -------------------------------------------------------------------		
	case CC_Slider: {
		const QStyleOptionSlider *slider = qstyleoption_cast<const QStyleOptionSlider *>(opt);
		if (!slider)
			break;

		// Groove
		if (slider->subControls & SC_SliderGroove) {
			QRect groove = getScaledRect(proxy()->subControlRect(CC_Slider, slider, SC_SliderGroove, widget), dpr);
			p->save();
			if (isScaled) {
				p->scale(inverseScale, inverseScale);
				p->translate(0.5, 0.5);
			}

			p->setPen(cdata->bgShades[5]);
			p->setBrush(cdata->bgShades[3]);
			p->drawRect(groove.adjusted(0,0,-1,-1));				
			p->setPen(cdata->bgShades[4]);
			p->drawLine(groove.left()+1, groove.top()+1,
						groove.left()+1, groove.bottom()-1);
			p->drawLine(groove.left()+1, groove.top()+1,
						groove.right()-1, groove.top()+1);
			
			p->restore();
		}

		// Handle
		if (slider->subControls & SC_SliderHandle) {
			QRect handle = getScaledRect(proxy()->subControlRect(CC_Slider, slider, SC_SliderHandle, widget), dpr);
			bool hovered = false;
			if ((slider->state & State_MouseOver) && (slider->state & State_Enabled)) {
				QPoint mousePos = widget ? widget->mapFromGlobal(QCursor::pos()) : QPoint();
				SubControl sc = QCommonStyle::hitTestComplexControl(CC_Slider, slider, mousePos, widget);
				if (sc == SC_SliderHandle)
					hovered = true;
			}

			p->save();
			if (isScaled)
				p->scale(inverseScale, inverseScale);

			// Background area (draw it underneath the white bevel border to ensure no gaps appear in QtQuick
			p->fillRect(handle.adjusted(1,1,-1,-1),
						opt->palette.brush(hovered ? QPalette::Midlight : QPalette::Button));

			// Handle border
			p->setPen(cdata->btnShades[6]);
			p->drawLine(handle.x() + 2, handle.y(),
						handle.right() - 2, handle.y());
			p->drawLine(handle.x(), handle.y() + 2,
						handle.x(), handle.bottom() - 2);
			p->drawLine(handle.right(), handle.y() + 2,
						handle.right(), handle.bottom() - 2);
			p->drawLine(handle.x() + 2, handle.bottom(),
						handle.right() - 2, handle.bottom());
			p->drawPoint(handle.x() + 1, handle.y() + 1);
			p->drawPoint(handle.right() - 1, handle.y() + 1);
			p->drawPoint(handle.right() - 1, handle.bottom() - 1);
			p->drawPoint (handle.x() + 1, handle.bottom() - 1);
			  
			// Handle bevel
			p->setPen(cdata->btnShades[2]);
			p->drawLine(handle.x() + 2, handle.bottom() - 1,
						handle.right() - 2, handle.bottom() - 1);
			p->drawLine(handle.right() - 1, handle.top() + 2,
						handle.right() - 1, handle.bottom() - 2);
			p->drawPoint (handle.x() + 1, handle.y());
			p->drawPoint (handle.right() - 1, handle.y());
			p->drawPoint (handle.x(), handle.y() + 1);
			p->drawPoint (handle.right(), handle.y() + 1);
			p->drawPoint (handle.x(), handle.bottom() - 1);
			p->drawPoint (handle.right(), handle.bottom() - 1);
			p->drawPoint (handle.x() + 1, handle.bottom() );
			p->drawPoint (handle.right() - 1, handle.bottom());			  
			p->setPen(Qt::white);
			p->drawLine (handle.x() + 2, handle.y() + 1,
						 handle.right() - 2, handle.y() + 1);
			p->drawLine (handle.x() + 1, handle.y() + 2,
						 handle.x() + 1, handle.bottom() - 2);

			// Stipple
			if (slider->orientation == Qt::Horizontal) {
				int x1 = handle.x() + handle.width() / 2 - 5;
				int y1 = handle.y() + (handle.height() - 7) / 2;

				p->setPen(cdata->btnShades[5]);
				p->drawLine(x1 + 0, y1 + 4,
							x1 + 3, y1 + 1);
				p->drawLine(x1 + 2, y1 + 6,
							x1 + 8, y1 + 0);
				p->drawLine(x1 + 7, y1 + 5,
							x1 + 10, y1 + 2);

				p->setPen(Qt::white);
				p->drawLine(x1 + 1, y1 + 4,
							x1 + 3, y1 + 2);
				p->drawLine(x1 + 3, y1 + 6,
							x1 + 8, y1 + 1);
				p->drawLine(x1 + 8, y1 + 5,
							x1 + 10, y1 + 3);
			} else {
				int x1 = handle.x() + (handle.width() - 7) / 2;
				int y1 = handle.y() + handle.height() / 2 - 5;

				p->setPen(cdata->btnShades[5]);
				p->drawLine(x1 + 4, y1 + 0,
							x1 + 1, y1 + 3);
				p->drawLine(x1 + 6, y1 + 2,
							x1 + 0, y1 + 8);
				p->drawLine(x1 + 5, y1 + 7,
							x1 + 2, y1 + 10);

				p->setPen(Qt::white);
				p->drawLine(x1 + 4, y1 + 1,
							x1 + 2, y1 + 3);
				p->drawLine(x1 + 6, y1 + 3,
							x1 + 1, y1 + 8);
				p->drawLine(x1 + 5, y1 + 8,
							x1 + 3, y1 + 10);
			}
			
			p->restore();
		}

		// Tick marks
		if (opt->subControls & SC_SliderTickmarks) {
			QStyleOptionSlider copy(*slider);
			copy.subControls = SC_SliderTickmarks;
			QCommonStyle::drawComplexControl(CC_Slider, &copy, p, widget);
		}

		// Focus rect
		if (opt->state & State_HasFocus) {
			QStyleOptionFocusRect focus;
			focus.QStyleOption::operator=(*slider);
			proxy()->drawPrimitive(PE_FrameFocusRect, &focus, p, widget);
		}
		
		break;
	}

	// TOOL BUTTON
	// -------------------------------------------------------------------		
	case CC_ToolButton: {
		const QStyleOptionToolButton *toolbutton = qstyleoption_cast<const QStyleOptionToolButton *>(opt);
		if (!toolbutton)
			break;

		// By default, Qt has both the main tool button and menu button highlight if the mouse hovers
		// over it. Instead, here we have the tool and menu buttons behave independantly, as they do
		// on GTK+2.0.

		const int fw = proxy()->pixelMetric(PM_DefaultFrameWidth, opt, widget);

		// Check which widget is being hovered by the mouse (if any)
		QPoint mousePos = widget ? widget->mapFromGlobal(QCursor::pos()) : QPoint();
		SubControl hoveredControl = QCommonStyle::hitTestComplexControl(CC_ToolButton, toolbutton, mousePos, widget);

		// Main toolbutton
		if (toolbutton->subControls & SC_ToolButton) {
		    QStyleOptionButton button;
			button.QStyleOption::operator=(*toolbutton);
			button.rect = proxy()->subControlRect(CC_ToolButton, toolbutton, SC_ToolButton, widget);

			// Set state flags
			if (!(toolbutton->activeSubControls == SC_ToolButton) && (toolbutton->subControls & SC_ToolButtonMenu))
				button.state &= ~(State_Sunken | State_On);
			if (!((toolbutton->state & State_Enabled) && (hoveredControl == SC_ToolButton)))
				button.state &= ~State_MouseOver;
			if (button.state & State_AutoRaise && (!(button.state & State_MouseOver) || !(button.state & State_Enabled)))
				button.state &= ~State_Raised;

			// Draw the button
			if (button.state & (State_Sunken | State_On | State_Raised))
				proxy()->drawPrimitive(PE_PanelButtonTool, &button, p, widget);

			// Draw the label			
			QStyleOptionToolButton label = *toolbutton;
			label.state = button.state;
			label.rect = button.rect.adjusted(fw, fw, -fw, -fw);
			proxy()->drawControl(CE_ToolButtonLabel, &label, p, widget);

			// If there is no menu button, but a submenu is present, we draw an indicator arrow
			// directly on the main tool button
			if (toolbutton->features & QStyleOptionToolButton::HasMenu && !(toolbutton->subControls & SC_ToolButtonMenu)) {
				int mbi = proxy()->pixelMetric(PM_MenuButtonIndicator, toolbutton, widget) - 5;
				QStyleOption arrow = button;
				arrow.rect = QRect(button.rect.right() + 1 - fw - mbi, button.rect.bottom() + 1 - fw - mbi, mbi, mbi);
				arrow.rect = visualRect(toolbutton->direction, button.rect, arrow.rect);
				proxy()->drawPrimitive(PE_IndicatorArrowDown, &arrow, p, widget);
			}
		}

		// Menu button
		if (toolbutton->subControls & SC_ToolButtonMenu) {
		    QStyleOptionButton button;
			button.QStyleOption::operator=(*toolbutton);
			button.rect = proxy()->subControlRect(CC_ToolButton, toolbutton, SC_ToolButtonMenu, widget);

			// Set state flags
		    if (!(toolbutton->activeSubControls == SC_ToolButtonMenu) && (toolbutton->subControls & SC_ToolButton))
				button.state &= ~(State_Sunken | State_On);
			if (!((toolbutton->state & State_Enabled) && (hoveredControl == SC_ToolButtonMenu)))
				button.state &= ~State_MouseOver;
			if (button.state & State_AutoRaise && (!(button.state & State_MouseOver) || !(button.state & State_Enabled)))
				button.state &= ~State_Raised;

			// Draw the button
			if (button.state & (State_Sunken | State_On | State_Raised))
				proxy()->drawPrimitive(PE_IndicatorButtonDropDown, &button, p, widget);

			// Draw the indicator arrow
			QStyleOption arrow = button;
			arrow.rect.adjust(fw, fw, -fw, -fw);
			proxy()->drawPrimitive(PE_IndicatorArrowDown, &arrow, p, widget);
		}
		break;
	}
		
	default: {
		QCommonStyle::drawComplexControl(control, opt, p, widget);
		break;
	}
	}
}

QRect
BluecurveStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *opt,
							   SubControl sc, const QWidget *widget) const
{
	QRect ret;
	
	switch (control) {
	// SCROLLBAR
	// -------------------------------------------------------------------
	case CC_ScrollBar: {
		/* taken from qcommonstyle.cpp */
		if (const QStyleOptionSlider *scrollbar = qstyleoption_cast<const QStyleOptionSlider *>(opt)) {
            const QRect scrollBarRect = scrollbar->rect;
            int sbextent = pixelMetric(PM_ScrollBarExtent, scrollbar, widget);
            int maxlen = ((scrollbar->orientation == Qt::Horizontal) ?
                          scrollBarRect.width() : scrollBarRect.height()) - (sbextent * 2) + 2;
            int sliderlen;

            // calculate slider length
            if (scrollbar->maximum != scrollbar->minimum) {
                uint range = scrollbar->maximum - scrollbar->minimum;
                sliderlen = (qint64(scrollbar->pageStep) * maxlen) / (range + scrollbar->pageStep);

                int slidermin = proxy()->pixelMetric(PM_ScrollBarSliderMin, scrollbar, widget);
                if (sliderlen < slidermin || range > INT_MAX / 2)
                    sliderlen = slidermin;
                if (sliderlen > maxlen)
                    sliderlen = maxlen;
            } else {
                sliderlen = maxlen;
            }

            int sliderstart = sbextent - 1 + sliderPositionFromValue(scrollbar->minimum,
                                                                 scrollbar->maximum,
                                                                 scrollbar->sliderPosition,
                                                                 maxlen - sliderlen,
                                                                 scrollbar->upsideDown);

            switch (sc) {
            case SC_ScrollBarSubLine:            // top/left button
                if (scrollbar->orientation == Qt::Horizontal) {
                    int buttonWidth = qMin(scrollBarRect.width() / 2, sbextent);
                    ret.setRect(0, 0, buttonWidth, scrollBarRect.height());
                } else {
                    int buttonHeight = qMin(scrollBarRect.height() / 2, sbextent);
                    ret.setRect(0, 0, scrollBarRect.width(), buttonHeight);
                }
                break;
            case SC_ScrollBarAddLine:            // bottom/right button
                if (scrollbar->orientation == Qt::Horizontal) {
                    int buttonWidth = qMin(scrollBarRect.width()/2, sbextent);
                    ret.setRect(scrollBarRect.width() - buttonWidth, 0, buttonWidth, scrollBarRect.height());
                } else {
                    int buttonHeight = qMin(scrollBarRect.height()/2, sbextent);
                    ret.setRect(0, scrollBarRect.height() - buttonHeight, scrollBarRect.width(), buttonHeight);
                }
                break;
            case SC_ScrollBarSubPage:            // between top/left button and slider
                if (scrollbar->orientation == Qt::Horizontal)
                    ret.setRect(sbextent, 0, sliderstart - sbextent, scrollBarRect.height());
                else
                    ret.setRect(0, sbextent, scrollBarRect.width(), sliderstart - sbextent);
                break;
            case SC_ScrollBarAddPage:            // between bottom/right button and slider
                if (scrollbar->orientation == Qt::Horizontal)
                    ret.setRect(sliderstart + sliderlen, 0,
                                maxlen - sliderstart - sliderlen + sbextent - 2, scrollBarRect.height());
                else
                    ret.setRect(0, sliderstart + sliderlen, scrollBarRect.width(),
                                maxlen - sliderstart - sliderlen + sbextent - 2);
                break;
            case SC_ScrollBarGroove:
                if (scrollbar->orientation == Qt::Horizontal)
                    ret.setRect(sbextent, 0, scrollBarRect.width() - sbextent * 2,
                                scrollBarRect.height());
                else
                    ret.setRect(0, sbextent, scrollBarRect.width(),
                                scrollBarRect.height() - sbextent * 2);
                break;
            case SC_ScrollBarSlider:
                if (scrollbar->orientation == Qt::Horizontal)
                    ret.setRect(sliderstart, 0, sliderlen, scrollBarRect.height());
                else
                    ret.setRect(0, sliderstart, scrollBarRect.width(), sliderlen);
                break;
            default:
                break;
            }
            ret = visualRect(scrollbar->direction, scrollBarRect, ret);
        }
        break;
	}

	// COMBOBOX
	// -------------------------------------------------------------------		
	case CC_ComboBox: {
		ret = QCommonStyle::subControlRect(control, opt, sc, widget);
	    bool reverse = (opt->direction == Qt::RightToLeft);
		switch (sc) {
		case SC_ComboBoxArrow: {
			ret.adjust(reverse ? 0 : -1, 0, reverse ? 1 : 0, 0);
			break;
		}
		case SC_ComboBoxEditField: {
			ret.adjust(reverse ? 3 : 0, 0, reverse ? 0 : -3, 0);
			break;
		}
		default: {
			break;
		}
		}
		break;
	}

	// SPINBOX
	// -------------------------------------------------------------------		
	case CC_SpinBox: {		
		const QStyleOptionSpinBox *spinbox = qstyleoption_cast<const QStyleOptionSpinBox *>(opt);
		if (!spinbox)
			break;

		// Button size
		QSize bs;
		bs.setHeight(spinbox->rect.height()/2 + 1); // We add 1 to account for the overlap
		bs.setWidth( bs.height() + 1);

		// Button coordinates
		int x = spinbox->rect.x() + spinbox->rect.width() - bs.width();
		int y = spinbox->rect.y();
		
		switch ( sc ) {
		case SC_SpinBoxUp: {
			if (spinbox->buttonSymbols == QAbstractSpinBox::NoButtons)
				break;
			ret.setRect(x, y, bs.width(), bs.height());
			break;
		}
		case SC_SpinBoxDown: {
			if (spinbox->buttonSymbols == QAbstractSpinBox::NoButtons)
				break;
			ret.setRect(x, y + bs.height() - 1, bs.width(), bs.height());
			break;
		}
		case SC_SpinBoxEditField: {
			if (spinbox->buttonSymbols == QAbstractSpinBox::NoButtons)
				ret = spinbox->rect;
			else
				ret = spinbox->rect.adjusted(0, 0, -bs.width() + 1, 0);
			break;
		}
		case SC_SpinBoxFrame: {
			ret = spinbox->rect;
		}
		default: {
			break;
		}
		}
		
		break;
	}

	// SLIDER
	// -------------------------------------------------------------------		
	case CC_Slider: {
		const QStyleOptionSlider *slider = qstyleoption_cast<const QStyleOptionSlider *>(opt);
		if (!slider)
			break;

		QStyleOptionSlider copy(*slider);
		copy.rect.adjust(2,2,-2,-2); // Apply 2px of padding to the entire region to account for the focus rect
		ret = QCommonStyle::subControlRect(CC_Slider, &copy, sc, widget); // Query the SC rect with modified opt

		// Force the groove to 5px width
		if (sc == SC_SliderGroove) {
			QPoint center = ret.center();
			if (slider->orientation == Qt::Horizontal)
				ret.setHeight(5);
			else
				ret.setWidth(5);
			ret.moveCenter(center);
		}
		
		break;
	}

	default: {
		ret = QCommonStyle::subControlRect(control, opt, sc, widget);
		break;
	}
	}

	return ret;
}

int
BluecurveStyle::pixelMetric(PixelMetric metric, const QStyleOption *opt,
							const QWidget *widget) const
{
	int ret = 0;

	switch (metric) {
	// BUTTONS
	// -------------------------------------------------------------------
	case PM_ButtonMargin:
		ret = 10;
		break;
		
	case PM_ButtonDefaultIndicator:
		ret = 0;
		break;

	case PM_MenuButtonIndicator:
		ret = opt ? std::max(12, (opt->rect.height() - 4) / 3) : 12;
		break;

	case PM_ButtonShiftHorizontal:
	case PM_ButtonShiftVertical:
		ret = 0;
		break;

	case PM_ButtonIconSize:
		ret = 20;
		break;

	// CHECKBOXES / RADIO BUTTONS
	// -------------------------------------------------------------------
	case PM_IndicatorWidth:
	case PM_IndicatorHeight:
	case PM_ExclusiveIndicatorWidth:
	case PM_ExclusiveIndicatorHeight:
		ret = 13;
		break;

	// SPLITTERS
	// -------------------------------------------------------------------
	case PM_DockWidgetSeparatorExtent:
	case PM_SplitterWidth:
		ret = 6;
		break;

	case PM_DockWidgetHandleExtent:
		ret = 10;
		break;

	// TABS
	// -------------------------------------------------------------------
	case PM_TabBarTabOverlap:
		ret = 1;
		break;

	case PM_TabBarTabHSpace:
		ret = 11;
		break;

	case PM_TabBarTabVSpace:
		ret = 13;
		break;

	case PM_TabBarBaseHeight:
		ret = 0;
		break;

	case PM_TabBarBaseOverlap:
		ret = 2;
		break;

	case PM_TabBarTabShiftVertical:
		ret = 0;
		break;

	// MENUS
	// -------------------------------------------------------------------
	case PM_MenuBarPanelWidth:
		ret = 1;
		break;

	case PM_MenuPanelWidth:
		ret = 3;
		break;
		
	case PM_MenuVMargin:
		ret = 1;
		break;

	case PM_SubMenuOverlap:
		ret = 2;
		break;

	// SCROLLBAR
	// ------------------------------------------------------------------------
	case PM_ScrollBarExtent:
		ret = 15;
		break;
		
	case PM_ScrollBarSliderMin:
		ret = 31;
		break;

	// SLIDER
	// ------------------------------------------------------------------------
	case PM_SliderControlThickness: {
	    const QStyleOptionSlider *sl = qstyleoption_cast<const QStyleOptionSlider *>(opt);
		if (!sl)
			break;
		
	    int space = (sl->orientation == Qt::Horizontal) ? sl->rect.height() : sl->rect.width();
	    int ticks = sl->tickPosition;
	    int n = 0;
	    if ( ticks & QSlider::TicksAbove ) n++;
	    if ( ticks & QSlider::TicksBelow ) n++;
	    if ( !n ) {
			ret = space;
			break;
	    }

		int thick = 6;        // Magic constant to get 5 + 16 + 5
		if (ticks != QSlider::TicksBothSides && ticks != QSlider::NoTicks)
			thick += proxy()->pixelMetric(PM_SliderLength, sl, widget) / 4;

		space -= thick;
		if (space > 0)
			thick += (space * 2) / (n + 2);
		ret = thick;
		break;
	}
		
	case PM_SliderLength: {
		ret=31;
		const QStyleOptionSlider *slider = qstyleoption_cast<const QStyleOptionSlider *>(opt);
		if (!slider)
			break;
		
		if (slider->orientation == Qt::Horizontal) {
			if (slider->rect.width()<ret)
				ret = slider->rect.width();
		} else {
			if (slider->rect.height()<ret)
				ret = slider->rect.height();
		}
		break;
	}

	case PM_SliderThickness:
		ret = 19;
		break;

	// GENERAL
	// ------------------------------------------------------------------------
		
	case PM_MaximumDragDistance:
		ret = -1;
		break;

	case PM_ToolBarItemSpacing:
		ret = 0;
		break;

	case PM_ProgressBarChunkWidth:
		ret = 2;
		break;

	case PM_HeaderMarkSize:
		ret = 32;
		break;
		
	default:
		ret = QCommonStyle::pixelMetric(metric, opt, widget);
		break;
	}

	return ret;
}

QSize
BluecurveStyle::sizeFromContents(ContentsType contents,
								 const QStyleOption *opt,
								 const QSize &contentsSize,
								 const QWidget *widget) const
{
	QSize ret = QCommonStyle::sizeFromContents( contents, opt, contentsSize, widget );

	switch (contents) {
	case CT_PushButton: {
		const QStyleOptionButton *buttonOpt = qstyleoption_cast<const QStyleOptionButton *>(opt);
		int w = ret.width(), h = ret.height();

		// only expand the button if we are displaying text...
		if (buttonOpt->icon.isNull()) {
			if ( w < 85 )
				w = 85;
			if ( h < 30 )
				h = 30;
		}
		
		ret = QSize( w, h );
		break;
	}

	case CT_MenuItem:
	case CT_MenuBarItem: {
		const QStyleOptionMenuItem *miOpt = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
		int w = contentsSize.width(), h = contentsSize.height();

		if (miOpt->menuItemType == QStyleOptionMenuItem::Separator) {
			w = 10;
			h = 12;
		} else {
			// check is at least 16x16
			if (h < 16) {
				h = 16;
			}

			h = std::max({h, opt->fontMetrics.height() + 10, pixelMetric(PM_SmallIconSize) + 8});
		}

		// check is at least 16x16
		if (contents == CT_MenuItem)
			w += std::max(miOpt->maxIconWidth, 16) + 16;
		else
			w += 16;

		if (!miOpt->text.isNull() && miOpt->text.indexOf('\t') >= 0)
			w += 8;

		ret = QSize(w, h);
		break;
	}

	case CT_ToolButton: {
		int w = ret.width(), h = ret.height();
		h = (h < 32 ? 32 : h);
		w = (w < 32 ? 32 : w);
		ret = QSize(w, h);
		break;
	}

	case CT_ComboBox: {
		int w = ret.width(), h = ret.height();
		if (h < 27)
			h = 27;
		ret = QSize(w, h);
		break;
	}

	case CT_SpinBox: {
		int w = ret.width(), h = ret.height();
		if (h < 25)
			h = 25;

		ret = QSize(w, h);
		break; 
	}

	case CT_SizeGrip: {
		int size = std::max({ret.width(), ret.height(), 18});
		ret = QSize(size,size);
		break;
	}

	case CT_Slider: {
		const QStyleOptionSlider *slider = qstyleoption_cast<const QStyleOptionSlider *>(opt);
		if (!slider)
			break;

		int w = ret.width(), h = ret.height();
		
		if (slider->orientation == Qt::Horizontal) {
			if (h < 17)
				h = 17;	
		} else {
			if (w < 17)
				w = 17;	
		}

		ret = QSize(w,h);
		break;
	}
		
	default: {
		break;
	}
	}

	return ret;
}

int
BluecurveStyle::styleHint(StyleHint sh, const QStyleOption *opt,
						  const QWidget *widget,
						  QStyleHintReturn *hret) const
{
	int ret;

	switch (sh) {
	case SH_EtchDisabledText:
	case SH_ScrollBar_MiddleClickAbsolutePosition:
	case SH_Slider_SnapToValue:
	case SH_PrintDialog_RightAlignButtons:
	case SH_FontDialog_SelectAssociatedText:
	case SH_Menu_SpaceActivatesItem:
	case SH_MenuBar_AltKeyNavigation:
	case SH_Menu_MouseTracking:
	case SH_MenuBar_MouseTracking:
	case SH_ComboBox_ListMouseTracking:
	case SH_UnderlineShortcut:
	case SH_ToolBar_Movable: {
		ret = 1;
		break;
	}
		
	case SH_MainWindow_SpaceBelowMenuBar:
	case SH_Menu_AllowActiveAndDisabled: {
		ret = 0;
		break;
	}
	
	default: {
		ret = QCommonStyle::styleHint(sh, opt, widget, hret);
		break;
	}
	}
	
	return ret;
}

void
BluecurveStyle::drawGradient(QPainter *p, QRect const &rect,
							 const QPalette &palette,
							 double shade1, double shade2,
							 bool horiz) const
{
	QColor c, c1, c2;
	int r, g, b;
	int c2r, c2g, c2b;
	int dr, dg, db, size;
	int start, end, left, right, top, bottom;

	left = rect.left();
	top = rect.top();
	bottom = rect.bottom();
	right = rect.right();

	start = horiz ? left : top;
	end = horiz ? right : bottom;

	if (end == start)
		return;

	shade (palette.highlight().color(), c1, shade1);
	shade (palette.highlight().color(), c2, shade2);

	c1.getRgb(&r, &g, &b);
	c2.getRgb(&c2r, &c2g, &c2b);

	size = end - start;
	dr = (c2r - r) / size;
	dg = (c2g - g) / size;
	db = (c2b - b) / size;

	for (int i = start; i <= end; i++) {
		c.setRgb (r, g, b);
		p->setPen(c);
		if (horiz)
			p->drawLine(i, top, i, bottom);
		else
			p->drawLine(left, i, right, i);

		r += dr;
		g += dg;
		b += db;
	}	

}

void
BluecurveStyle::drawGradientBox(QPainter *p, const QStyleOption *opt,
								const BluecurveColorData *cdata,
								bool horiz,
								double shade1, double shade2) const
{
	p->save();
	
	const qreal dpr = getDpr(p);
	QRect r;
	if (!qFuzzyCompare(dpr, qreal(1))) {
		const qreal inverseScale = qreal(1) / dpr;
		p->scale(inverseScale, inverseScale);
		p->translate(0.5, 0.5);
	    r = getScaledRect(opt->rect, dpr);
	} else {
		r = opt->rect;
	}
	
	QRect grad(r.left()+2, r.top()+2, r.width()-3, r.height()-3);
	drawGradient(p, grad, opt->palette, shade1, shade2, horiz);

// 3d border effect...
	p->setPen(cdata->spots[2]);
	p->setBrush(Qt::NoBrush);
	p->drawRect(r.adjusted(0,0,-1,-1));

//	We draw the bottom and right lines first ...
	p->setPen(cdata->spots[1]);
	p->drawLine(r.left()+1, r.bottom()-1, r.right()-1, r.bottom()-1);
	p->drawLine(r.right()-1, r.top()+1, r.right()-1, r.bottom()-1);

//	Because the lighter lines should overlap them on the corner pixels
	p->setPen(cdata->spots[0]);
	p->drawLine(r.left()+1, r.top()+1, r.right()-1, r.top()+1);
	p->drawLine(r.left()+1, r.top()+1, r.left()+1, r.bottom()-1);

	p->restore();
}

void
BluecurveStyle::tabLayout(const QStyleOptionTab *opt, const QWidget *widget,
						  QRect *textRect, QRect *iconRect) const
{
	Q_ASSERT(textRect);
	Q_ASSERT(iconRect);
	QRect tr = opt->rect;
	bool verticalTabs = opt->shape == QTabBar::RoundedEast
		|| opt->shape == QTabBar::RoundedWest
		|| opt->shape == QTabBar::TriangularEast
		|| opt->shape == QTabBar::TriangularWest;
	if (verticalTabs)
		tr.setRect(0, 0, tr.height(), tr.width()); // 0, 0 as we will have a translate transform

	int verticalShift = pixelMetric(QStyle::PM_TabBarTabShiftVertical, opt, widget);
	int horizontalShift = pixelMetric(QStyle::PM_TabBarTabShiftHorizontal, opt, widget);
	int hpadding = pixelMetric(QStyle::PM_TabBarTabHSpace, opt, widget) / 2;
	int vpadding = pixelMetric(QStyle::PM_TabBarTabVSpace, opt, widget) / 2;
	if (opt->shape == QTabBar::RoundedSouth || opt->shape == QTabBar::TriangularSouth)
		verticalShift = -verticalShift;
	tr.adjust(hpadding, verticalShift + vpadding, horizontalShift - hpadding, -vpadding);
	bool selected = opt->state & QStyle::State_Selected;
	if (selected) {
		tr.setTop(tr.top() - verticalShift);
		tr.setRight(tr.right() - horizontalShift);
	}

    // left widget
	if (!opt->leftButtonSize.isEmpty()) {
		tr.setLeft(tr.left() + 4 +
				   (verticalTabs ? opt->leftButtonSize.height() : opt->leftButtonSize.width()));
	}
    // right widget
	if (!opt->rightButtonSize.isEmpty()) {
		tr.setRight(tr.right() - 4 -
					(verticalTabs ? opt->rightButtonSize.height() : opt->rightButtonSize.width()));
	}

    // icon
	if (!opt->icon.isNull()) {
		QSize iconSize = opt->iconSize;
		if (!iconSize.isValid()) {
			int iconExtent = pixelMetric(QStyle::PM_SmallIconSize, opt, widget);
			iconSize = QSize(iconExtent, iconExtent);
        }
		QSize tabIconSize = opt->icon.actualSize(iconSize,
												 QIcon::Normal, // We always use QIcon::Normal always here for our style
												 (opt->state & QStyle::State_Selected) ? QIcon::On : QIcon::Off);
        // High-dpi icons do not need adjustment; make sure tabIconSize is not larger than iconSize
		tabIconSize = QSize(qMin(tabIconSize.width(), iconSize.width()), qMin(tabIconSize.height(), iconSize.height()));

		const int offsetX = (iconSize.width() - tabIconSize.width()) / 2;
		*iconRect = QRect(tr.left() + offsetX, tr.center().y() - tabIconSize.height() / 2,
						  tabIconSize.width(), tabIconSize.height());
		if (!verticalTabs)
			*iconRect = QStyle::visualRect(opt->direction, opt->rect, *iconRect);
		tr.setLeft(tr.left() + tabIconSize.width() + 4);
	}

	if (!verticalTabs)
		tr = QStyle::visualRect(opt->direction, opt->rect, tr);

	*textRect = tr;
}

/* Arrow drawing logic taken from Bluecurve GTK+ 2.0 engine
   Copyright: Garrett LeSage, Alexander Larsson */

void
BluecurveStyle::calculate_arrow_geometry(PrimitiveElement pe,
										 int &x,
										 int &y,
										 int &width,
										 int &height)
{
	int w = width;
	int h = height;

	switch (pe) {
    case PE_IndicatorArrowUp:
    case PE_IndicatorArrowDown:
		w += (w % 2) - 1;
		h = (w / 2 + 1) + 1;

		if (h > height)
		{
			h = height;
			w = 2 * (h - 1) - 1;
		}
      
		if (pe == PE_IndicatorArrowDown)
		{
			if (height % 2 == 1 || h % 2 == 0)
				height += 1;
		}
		else
		{
			if (height % 2 == 0 || h % 2 == 0)
				height -= 1;
		}
		break;

    case PE_IndicatorArrowRight:
    case PE_IndicatorArrowLeft:
		h += (h % 2) - 1;
		w = (h / 2 + 1) + 1; 
      
		if (w > width)
		{
			w = width;
			h = 2 * (w - 1) - 1;
		}
      
		if (pe == PE_IndicatorArrowRight)
		{
			if (width % 2 == 1 || w % 2 == 0)
				width += 1;
		}
		else
		{
			if (width % 2 == 0 || w % 2 == 0)
				width -= 1;
		}
		break;
      
    default:

		break;
    }

	x += (width - w) / 2;
	y += (height - h) / 2;
	height = h;
	width = w;
}

void
BluecurveStyle::drawArrow(QPainter *p,
						  PrimitiveElement pe,
						  int x,
						  int y,
						  int width,
						  int height)
{
	int i, j;
	
	switch (pe) {
	case PE_IndicatorArrowDown: {
		for (i = 0, j = -1; i < height; i++, j++)
			arrow_draw_hline (p, x + j, x + width - j - 1, y + i, i == 0);
		break;
	}

	case PE_IndicatorArrowUp: {
		for (i = height - 1, j = -1; i >= 0; i--, j++)
			arrow_draw_hline (p, x + j, x + width - j - 1, y + i, i == height - 1);
		break;
	}

	case PE_IndicatorArrowLeft: {
		for (i = width - 1, j = -1; i >= 0; i--, j++)
			arrow_draw_vline (p, y + j, y + height - j - 1, x + i, i == width - 1);
		break;
	}

	case PE_IndicatorArrowRight: {
		for (i = 0, j = -1; i < width; i++, j++)
			arrow_draw_vline (p, y + j, y + height - j - 1,  x + i, i == 0);
		break;
	}
			
	default: {
		break;
	}
	}
}

void
BluecurveStyle::arrow_draw_hline(QPainter *p,
								 int x1,
								 int x2,
								 int y,
								 bool last)
{
	if (x2 - x1 < 7 && !last)
		p->drawLine(x1, y, x2, y);
	else if (last) {
		if (x2 - x1 <= 7) {
			p->drawPoint(x1+1, y);
			p->drawPoint(x2-1, y);
		}
		else {
			p->drawPoint(x1+2, y);
			p->drawPoint(x2-2, y);
		}
    }
	else {
		p->drawLine(x1, y, x1+2, y);
		p->drawLine(x2-2, y, x2, y);
    }
		
}

void
BluecurveStyle::arrow_draw_vline(QPainter *p,
								 int y1,
								 int y2,
								 int x,
								 bool last)
{
	if (y2 - y1 < 7 && !last)
		p->drawLine(x, y1, x, y2);
	else if (last) {
		p->drawPoint(x, y1+2);
		p->drawPoint(x, y2-2);
    }
	else {
		p->drawLine(x, y1, x, y1+2);
		p->drawLine(x, y2-2, x, y2);
    }
}
