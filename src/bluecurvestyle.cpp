#include "bluecurvestyle.h"

#include <algorithm>

#include <QStyleFactory>
#include <QPainter>
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

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define RADIO_SIZE 13
#define CHECK_SIZE 13

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define CLAMP_UCHAR(v) ((unsigned char) (CLAMP (((int)v), (int)0, (int)255)))

#include "bits.cpp"

const double BluecurveStyle::shadeFactors[8] = {1.065, 0.963, 0.896, 0.85, 0.768, 0.665, 0.4, 0.205};

static void
rgb_to_hls (double *r, double *g, double *b) {
	double min;
	double max;
	double red;
	double green;
	double blue;
	double h, l, s;
	double delta;

	red = *r;
	green = *g;
	blue = *b;

	if (red > green) {
		if (red > blue)
			max = red;
		else
			max = blue;

		if (green < blue)
			min = green;
		else
			min = blue;
	} else {
		if (green > blue)
			max = green;
		else
			max = blue;

		if (red < blue)
			min = red;
		else
			min = blue;
	}

	l = (max + min) / 2;
	s = 0;
	h = 0;

	if (max != min) {
		if (l <= 0.5)
			s = (max - min) / (max + min);
		else
			s = (max - min) / (2 - max - min);

		delta = max -min;
		if (red == max)
			h = (green - blue) / delta;
		else if (green == max)
			h = 2 + (blue - red) / delta;
		else if (blue == max)
			h = 4 + (red - green) / delta;

		h *= 60;
		if (h < 0.0)
			h += 360;
	}

	*r = h;
	*g = l;
	*b = s;
}

static void
hls_to_rgb (double *h, double *l, double *s) {
	double hue;
	double lightness;
	double saturation;
	double m1, m2;
	double r, g, b;

	lightness = *l;
	saturation = *s;

	if (lightness <= 0.5)
		m2 = lightness * (1 + saturation);
	else
		m2 = lightness + saturation - lightness * saturation;
	m1 = 2 * lightness - m2;

	if (saturation == 0) {
		*h = lightness;
		*l = lightness;
		*s = lightness;
	} else {
		hue = *h + 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;

		if (hue < 60)
			r = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			r = m2;
		else if (hue < 240)
			r = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			r = m1;

		hue = *h;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;

		if (hue < 60)
			g = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			g = m2;
		else if (hue < 240)
			g = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			g = m1;

		hue = *h - 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;

		if (hue < 60)
			b = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			b = m2;
		else if (hue < 240)
			b = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			b = m1;

		*h = r;
		*l = g;
		*s = b;
	}
}

static void
shade (const QColor &ca, QColor &cb, double k) {
	int r,g,b;
	double red;
	double green;
	double blue;

	ca.getRgb(&r, &g, &b);

	red = (double) r / 255.0;
	green = (double) g / 255.0;
	blue = (double) b / 255.0;

	rgb_to_hls (&red, &green, &blue);

	green *= k;
	if (green > 1.0)
		green = 1.0;
	else if (green < 0.0)
		green = 0.0;

	blue *= k;
	if (blue > 1.0)
		blue = 1.0;
	else if (blue < 0.0)
		blue = 0.0;

	hls_to_rgb (&red, &green, &blue);

	cb.setRgb ((int)(red*255.0), (int)(green*255.0), (int)(blue*255.0));
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
	 r = MIN(r, 255);
	 g = (int) (color.green() * mult);
	 g = MIN(g, 255);
	 b = (int) (color.blue() * mult);
	 b = MIN(b, 255);
  
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
	 int dest_rowstride;
	 int width, height;
	 unsigned char *dest_pixels;
  
	 image = new QImage (RADIO_SIZE, RADIO_SIZE, QImage::Format_ARGB32);

	 if (image == NULL)
		  return NULL;
  
	 dest_rowstride = image->bytesPerLine();
	 width = image->width();
	 height = image->height();
	 dest_pixels = image->bits();
  
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

BluecurveStyle::BluecurveStyle() : QCommonStyle(), m_dataCache ()
{	
	basestyle = QStyleFactory::create("Windows"); // Original theme used MotifPlus, but this is no longer available, so we use Windows.
	if (!basestyle)
		qFatal( "BluecurveStyle: couldn't find a base style!" );
}

BluecurveStyle::~BluecurveStyle()
{
	delete basestyle;
}

void
BluecurveStyle::polish(QWidget *widget)
{
	if (widget->inherits("QAbstractButton") ||
		widget->inherits("QComboBox"))
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

	for (i = 0; i < 8; i++) {
		shade (palette.button().color(), cdata->shades[i], shadeFactors[i]);
	}

	shade (palette.highlight().color(), cdata->spots[0], 1.62);
	shade (palette.highlight().color(), cdata->spots[1], 1.05);
	shade (palette.highlight().color(), cdata->spots[2], 0.72);

	QImage *dot, *inconsistent, *outline, *circle, *check, *base;

	dot = colorize_bit (dot_intensity, dot_alpha, palette.highlight().color());
	outline = generate_bit (outline_alpha, cdata->shades[6], 1.0);

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
				circle = generate_bit (circle_alpha, cdata->shades[1], 1.0);
			}

			composeImage (&composite, circle);
			delete circle;

			cdata->radioPix[i*4+j*2+0] = new QPixmap (QPixmap::fromImage(composite));

			composeImage (&composite, dot);
			cdata->radioPix[i*4+j*2+1] = new QPixmap (QPixmap::fromImage(composite));
		}
	}

	QImage mask = outline->createAlphaMask();
	cdata->radioMask = new QPixmap (QPixmap::fromImage(mask));

	check = generate_bit (check_alpha, palette.highlight().color(), 1.0);
	inconsistent = generate_bit (check_inconsistent_alpha, palette.highlight().color(), 1.0);

	for (i = 0; i < 2; i++) {
		if (i == 0) {
			base = generate_bit (check_base_alpha, QColor(Qt::white), 1.0);
		} else {
			base = generate_bit (check_base_alpha, cdata->shades[1], 1.0);
		}

		composite.fill (cdata->shades[6].rgb());
		composeImage (&composite, base);
		cdata->checkPix[i*3+0] = new QPixmap (QPixmap::fromImage(composite));

		composeImage (&composite, check);
		cdata->checkPix[i*3+1] = new QPixmap (QPixmap::fromImage(composite));

		composite.fill (cdata->shades[6].rgb());
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
BluecurveStyle::getShade (const QPalette &palette, int shadenr, QColor &res) const
{
	const BluecurveColorData *cdata = lookupData(palette);

	res = cdata->shades[shadenr];
}

void
BluecurveStyle::drawTextRect(QPainter *p, const QStyleOption *opt,
							 const QBrush *fill) const
{
	const QRect &r = opt->rect;
	QRect br = r;

	const BluecurveColorData *cdata = lookupData(opt->palette);

	p->setPen(cdata->shades[5]);
	p->drawRect(r);

	// button bevel
	p->setPen(opt->palette.light().color());
	p->drawLine(r.x() + r.width() - 2, r.y() + 2,
				r.x() + r.width() - 2, r.y() + r.height() - 3); // right
	p->drawLine(r.x() + 2, r.y() + r.height() - 2,
				r.x() + r.width() - 2, r.y() + r.height() - 2); // bottom

	p->setPen(cdata->shades[1]);
	p->drawLine(r.x() + 1, r.y() + 2,
				r.x() + 1, r.y() + r.height() - 2); // left
	p->drawLine(r.x() + 1, r.y() + 1,
				r.x() + r.width() - 2, r.y() + 1); // top

	br.adjust(2, 2, -2, -2);

	// fill
	if (fill) {
		p->fillRect(br, *fill);
	}
}

void
BluecurveStyle::drawLightBevel(QPainter *p, const QStyleOption *opt,
							   const QBrush *fill, bool dark) const
{
	const QRect &r = opt->rect;
	QRect br = r;
	QColor col;

	bool sunken = (opt->state & (QStyle::State_On | QStyle::State_Sunken));

	const BluecurveColorData *cdata = lookupData(opt->palette);

	p->setPen(dark ? cdata->shades[6] : cdata->shades[5]);
    p->drawRect(r.adjusted(0, 0, -1, -1));

	if (opt->state & (QStyle::State_On |
					  QStyle::State_Sunken | QStyle::State_Raised)) {
		// button bevel
		p->setPen(sunken ? Qt::white : cdata->shades[2]);
		p->drawLine(r.x() + r.width() - 2, r.y() + 2,
					r.x() + r.width() - 2, r.y() + r.height() - 3); // right
		p->drawLine(r.x() + 1, r.y() + r.height() - 2,
					r.x() + r.width() - 2, r.y() + r.height() - 2); // bottom

		p->setPen(sunken ? cdata->shades[2] : Qt::white);
		p->drawLine(r.x() + 1, r.y() + 2,
					r.x() + 1, r.y() + r.height() - 2); // left
		p->drawLine(r.x() + 1, r.y() + 1,
					r.x() + r.width() - 2, r.y() + 1); // top

		br.adjust(2, 2, -2, -2);
	} else {
		br.adjust(1, 1, -1, -1);
	}

	// fill
	if (fill) {
		p->fillRect(br, *fill);
	}
}

void
BluecurveStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
							  QPainter *p, const QWidget *widget) const
{

	// this isn't necessary, but it cleans up the code, and makes copy/pasting in Qt3 code easier...
	const QRect &r = opt->rect;

	const BluecurveColorData *cdata = lookupData(opt->palette);
	
	switch (pe) {

	case PE_IndicatorHeaderArrow: {
		// Little hack to always force State_Enabled, per the Qt3 theme
		QStyleOption optCopy(*opt);
		optCopy.state |= QStyle::State_Enabled;
		drawPrimitive((opt->state & State_UpArrow) ? PE_IndicatorArrowUp : PE_IndicatorArrowDown, &optCopy, p, widget);
		break;
	}
		
	case PE_PanelButtonCommand:
	case PE_PanelButtonBevel:
	case PE_PanelButtonTool: {
		const QBrush *fill;

		if (opt->state & QStyle::State_Enabled) {
			if (opt->state & (QStyle::State_On | QStyle::State_Sunken)) {
				fill = &opt->palette.brush(QPalette::Dark);
			} else {
				if (opt->state & QStyle::State_MouseOver)
					fill = &opt->palette.brush(QPalette::Midlight);
				else
					fill = &opt->palette.brush(QPalette::Button);
			}
			 
		} else
			fill = &opt->palette.brush(QPalette::Window);

		drawLightBevel(p, opt, fill, true);
		break;
	}

	case PE_FrameButtonBevel:
	case PE_FrameButtonTool: {
		drawLightBevel(p, opt, 0, true);
		break;
	}

	case PE_FrameDefaultButton: {
		p->setPen(opt->palette.shadow().color());
		p->setBrush(Qt::NoBrush);
		p->drawRect(r.adjusted(0,0,-1,-1));
		break;
	}

	case PE_IndicatorMenuCheckMark: {
		QPoint qp = QPoint(r.center().x() - RADIO_SIZE/4, 
						   r.center().y() - RADIO_SIZE/2);
		if (opt->state & State_Selected)
			p->drawPixmap(qp, *(cdata->checkMark[0]));
		else
			p->drawPixmap(qp, *(cdata->checkMark[1]));

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

		p->drawPixmap(r.topLeft(), *cdata->checkPix[pix]);
		break;
	}

	case PE_IndicatorRadioButton: {
		int pix = 0;
		if (opt->state & State_MouseOver) {
			pix += 4;
			p->fillRect(r, opt->palette.brush(QPalette::Midlight));
		} else {
			p->fillRect(r, opt->palette.brush(QPalette::Window));
		}

		if (opt->state & State_Sunken)
			pix += 2;
		if (opt->state & State_On)
			pix += 1;
		
		p->drawPixmap(r.topLeft(), *cdata->radioPix[pix]);
		
		break;			
	}

	case PE_IndicatorToolBarHandle: {
		QString title;
		if ( p && p->device()->devType() == QInternal::Widget ) {
			QWidget *w = (QWidget *) p->device();
			QWidget *p = w->parentWidget();
		}

		QStyle::State state = opt->state;
		state |= State_Raised;

		if (state & State_Horizontal) {
			p->fillRect(r, opt->palette.button().color());

			int yy = r.top() + 3;
			int nLines = (r.height() - 6) / 5;

			for (int i = 0; i < nLines; yy += 5, i++) {
				p->setPen(cdata->shades[5]);
				p->drawLine(1, yy + 3, 4, yy);
				p->setPen(opt->palette.light().color());
				p->drawLine(1, yy + 4, 4, yy + 1);
			}			
		} else {
			p->fillRect(r, opt->palette.button().color());

			int xx = r.left() + 3;
			int nLines = (r.width() - 5) / 4;

			for (int i = 0; i < nLines; xx += 4, i++) {
				p->setPen(cdata->shades[5]);
				p->drawLine(xx + 3, 1,
							xx, 1 + 3);
				p->setPen(opt->palette.light().color());
				p->drawLine(xx + 3, 2,
							xx + 1, 1 + 3);
			}
		}

		break;
	}

	case PE_IndicatorToolBarSeparator: {
		if (r.width() > 20 || r.height() > 20) {
			if (opt->state & State_Horizontal) {
				p->setPen(cdata->shades[5]);
				p->drawLine(r.left() + 1, r.top() + 6, r.left() + 1, r.bottom() - 6);
				p->setPen(cdata->shades[3]);
				p->drawLine(r.left() + 2, r.top() + 6, r.left() + 2, r.bottom() - 6);
			} else {
				p->setPen(cdata->shades[5]);
				p->drawLine(r.left() + 6, r.top() + 1, r.right() - 6, r.top() + 1);
				p->setPen(cdata->shades[3]);
				p->drawLine(r.left() + 6, r.top() + 2, r.right() - 6, r.top() + 2);
			}
		} else {
		    QCommonStyle::drawPrimitive(pe, opt, p, widget);
		}
		break;		
	}

	case PE_IndicatorDockWidgetResizeHandle: {
		p->fillRect(r, opt->palette.window().color());
		if (opt->state & State_Horizontal) {
			p->setPen(opt->palette.highlight().color().lighter());
			p->drawLine(r.left() + 1, r.top() + 1, r.right() - 1, r.top() + 1);
			p->setPen(opt->palette.highlight().color());
			p->drawLine(r.left() + 1, r.top() + 2, r.right() - 1, r.top() + 2);
			p->setPen(opt->palette.highlight().color().darker());
			p->drawLine(r.left() + 1, r.top() + 3, r.right() - 1, r.top() + 3);
		} else {
			p->setPen(opt->palette.highlight().color().lighter());
			p->drawLine(r.left() + 1, r.top() + 1, r.left() + 1, r.bottom() - 1);
			p->setPen(opt->palette.highlight().color());
			p->drawLine(r.left() + 2, r.top() + 1, r.left() + 2, r.bottom() - 1);
			p->setPen(opt->palette.highlight().color().darker());
			p->drawLine(r.left() + 3, r.top() + 1, r.left() + 3, r.bottom() - 1);
		}
		break;		
	}

	case PE_PanelLineEdit: {
		drawTextRect(p, opt);
		break;
	}		

	case PE_FrameTabWidget: {
		p->setPen(cdata->shades[6]);
		p->drawLine(r.left(),r.top(),r.left(),r.bottom());
		p->drawLine(r.left(),r.bottom(),r.right(),r.bottom());
		p->drawLine(r.right(),r.bottom(),r.right(),r.top());
		p->drawLine(r.left(),r.top(),r.right(),r.top());
		p->setPen(opt->palette.light().color());
		p->drawLine(r.left()+1,r.top(),r.left()+1,r.bottom()-1);
		p->drawLine(r.left()+1,r.bottom()-1,r.right()-1,r.bottom()-1);
		p->drawLine(r.right()-1,r.bottom()-1,r.right()-1,r.top()+1);
		p->drawLine(r.left(),r.top()+1,r.right()-1,r.top()+1);
		break;        
	}		

	case PE_Frame:
	case PE_FrameWindow: {
		QStyleOption optCopy(*opt);
		if ( ! (optCopy.state & State_Sunken ) )
			optCopy.state |= State_Raised;
			
		drawLightBevel(p, &optCopy);
		break;
	}

	case PE_FrameMenu: {
		drawLightBevel(p, opt);
		break;
	}

	case PE_FrameDockWidget:
	case PE_PanelMenuBar: {
		p->fillRect(r, opt->palette.button().color());
		p->setPen(opt->palette.mid().color());
		p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
		break;
	}

	case PE_FrameFocusRect: {
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
		break;
	}

	case PE_IndicatorProgressChunk: {
		drawGradientBox(p, r, opt->palette, cdata, false, 0.92,1.66);
		break;
	}

	case PE_IndicatorArrowUp:
	case PE_IndicatorArrowDown:
	case PE_IndicatorArrowRight:
	case PE_IndicatorArrowLeft: {
		QPolygon a;

		switch ( pe ) {
		case PE_IndicatorArrowUp: {
		    a.setPoints(11,   3,1,  0,-2,  -3,1,  -2,0,  -2,2,  0,-1,  2,1,  2,2,  0,0,  -1,1,  1,1);
			break;
		}

		case PE_IndicatorArrowDown: {
			a.setPoints(11,   3,-1,  0,2,  -3,-1,  -2,0,  -2,-2,  0,1,  2,-1,  2,-2,  0,0,  -1,-1,  1,-1);
			break;
		}

		case PE_IndicatorArrowRight: {
			a.setPoints(13,  0,-3,  -1,-2,  1,-2,  2,-1,  0,-1,  1,0,  3,0,  2,1,  0,1,  -1,2,  1,2,  0,3,  0,0);
			break;
		}

		case PE_IndicatorArrowLeft: {
			a.setPoints(13,  0,-3,  1,-2,  -1,-2,  -2,-1,  0,-1,  -1,0,  -3,0,  -2,1,  0,1,  1,2,  -1,2,  0,3,  0,0);
			break;
		}

		default: {
			break;
		}
		}

		if (a.isEmpty())
			return;

		p->save();
		a.translate( (r.x() + r.width() / 2), 
					 (r.y() + r.height() / 2));
		
		if ( opt->state & State_Enabled )
			p->setPen( opt->state & (State_Selected | State_MouseOver) ? opt->palette.highlightedText().color() : opt->palette.buttonText().color());
		else
			p->setPen(cdata->shades[7]);

		p->drawPolyline(a);
		p->restore();		
		
		break;
	}

	case PE_IndicatorSpinUp:
	case PE_IndicatorSpinDown: {
		p->setPen(cdata->shades[7]);
		p->setBrush(Qt::NoBrush);
		QPolygon a;
		if (pe==PE_IndicatorSpinUp)
			a.setPoints(8,  -3,0,  -1,-2,  1,0,  1,1,  -1,-1,  -3,1,  -3,0,  1,0);
		else
			a.setPoints(8,  -3,-1,  -1,1,  1,-1,  1,-2,  -1,0,  -3,-2,  -3,-1,  1,-1);
		
		a.translate(r.x() + r.width() / 2, r.y() + r.height() / 2 + 1);
		p->drawPolyline(a);
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
	const QRect &r = opt->rect;
	
	const BluecurveColorData *cdata = lookupData(opt->palette);
	
	switch (control) {
		/*case CE_PushButtonLabel: {
		const QPushButton *button = (const QPushButton *) widget;
		const QStyleOptionButton *buttonOpt = (const QStyleOptionButton *) opt; // Needed to check button options
		QRect ir = r;

		if (button->isDown() || button->isChecked()) {
			flags |= State_Sunken;
			ir.translate(pixelMetric(PM_ButtonShiftHorizontal, opt, widget), pixelMetric(PM_ButtonShiftVertical, opt, widget));
		}

		if (buttonOpt->features & QStyleOptionButton::HasMenu) {
			int mbi = pixelMetric(PM_MenuButtonIndicator, opt, widget);
			QRect ar(ir.right() - mbi, ir.y() + 2, mbi - 4, ir.height() - 4);

			QStyleOption arrowOpt; // need to create a new styleoption for the arrow
			arrowOpt.rect = ar;
			arrowOpt.state = flags;
			arrowOpt.palette = palette;
			
		    drawPrimitive(PE_IndicatorArrowDown, &arrowOpt, p, widget);
			ir.setWidth(ir.width() - mbi);
		}

		int tf = Qt::AlignVCenter | Qt::TextShowMnemonic;
		//if (!styleHint(SH_UnderlineAccelerator, widget, QStyleOption::Default, 0))
		//tf |= NoAccel; seems to be entirely deprecated in Qt6, so we ignore it...

		if (!button->icon().isNull()) {
			QIcon::Mode mode =
				button->isEnabled() ? QIcon::Normal : QIcon::Disabled;
			if ( mode == QIcon::Normal && button->hasFocus() )
				mode = QIcon::Active;

			QIcon::State state = QIcon::Off;
			if ( button->isCheckable() && button->isChecked() )
				state = QIcon::On;

			QPixmap pixmap = button->icon().pixmap( pixelMetric(PM_SmallIconSize, opt, widget), mode, state );
			int pixw = pixmap.width();
			int pixh = pixmap.height();
			
                // content rectangle is pixmap + spacer + text bounding rectangle

			int csp = -2; // content spacer (between pixmap and text)
			QFontMetrics fm = p->fontMetrics();
			int contw;
			if (!button->text().isEmpty())
				contw = pixw + fm.horizontalAdvance(button->text()) + csp; // width of pixmap + text
			else
				contw = pixw;
			int contx = ir.x() + ir.width()/2 - contw/2; // upper left corner of content rectangle

			//Center the icon if there is no text
			if ( button->text().isEmpty() )
				p->drawPixmap( ir.x() + ir.width() / 2 - pixw / 2, ir.y() + ir.height() / 2 - pixh / 2, pixmap );
			else 
				p->drawPixmap( contx, ir.y() + ir.height() / 2 - pixh / 2, pixmap );
			ir.setLeft(contx);
			ir.setWidth(contw);
			
			// right-align text in the content rectangle, if there is any
			if (!button->text().isEmpty())
				tf |= Qt::AlignRight;		
		} else {
			tf |= Qt::AlignHCenter;
		}

		drawItemText(p, ir, tf, palette, (flags & State_Enabled), button->text(), QPalette::ButtonText);

		if (flags & State_HasFocus) {
			QStyleOption focusRectOpt;
			focusRectOpt.rect = subElementRect(SE_PushButtonFocusRect, opt, widget);
			focusRectOpt.state = flags;
			focusRectOpt.palette = palette;
			drawPrimitive(PE_FrameFocusRect, &focusRectOpt, p, widget);
		}
		
		break;
		}*/

	case CE_TabBarTabShape: {
		const QStyleOptionTab *tabOpt = qstyleoption_cast<const QStyleOptionTab *>(opt);
		bool below = false;
		QRect tr(r);
		QRect fr(r);

		tr.adjust(0, 0,  0, -1); // NB: adjust replaces addCoords
		fr.adjust(1, 2, -2, -2);

		if ( tabOpt->shape == QTabBar::RoundedSouth || tabOpt->shape == QTabBar::TriangularSouth) {
			tr = r; tr.adjust(0, 1, 0, 0);
			fr = r; fr.adjust(2, 2,-2, -2);
			below=true;
		}

		if (tr.left() != 0) {
			// ensure tabs borders overlap
			tr.adjust(-1,0,0,0);
			fr.adjust(-1,0,0,0);
		}

		if (! (opt->state & State_Selected)) {
			if (below) {
				tr.adjust(0, 0, 0, -1);
				fr.adjust(0, 0, 0, -1);
			} else {
				tr.adjust(0, 1, 0, 0);
				fr.adjust(0, 1, 0, 0);
			}
			
			if (tr.left() == 0)
				fr.adjust(1,0,0,0);

			p->setPen(cdata->shades[6]);
			p->drawRect(tr.adjusted(0,0,-1,-1));

			if (tr.left() == 0)
				if (below)
					p->drawPoint(tr.left(), tr.top() - 1);
				else
					p->drawPoint(tr.left(), tr.bottom() + 1);

			p->setPen(opt->palette.light().color());
			if (!below)
				p->drawLine(tr.left() + 1, tr.top() + 1, tr.right() - 1, tr.top() + 1);
			if (tr.left() == 0)
				p->drawLine(tr.left() + 1, tr.top() + 1, tr.left() + 1, tr.bottom() - 1);

			p->setPen(cdata->shades[2]);
			p->drawLine(tr.right() - 1, tr.top() + 1, tr.right() - 1, tr.bottom() - 1);
			if (below)
				p->drawLine(tr.left() + 2, tr.bottom() - 1, tr.right() - 1, tr.bottom() - 1);

		} else {

			fr.adjust(1,0,0,2); // ensure tab goes over the tab contents
			if (tr.left() != 0) {
				// selected tab borders move over to the left by 1px
				tr.adjust(-1,0,0,0);
				fr.adjust(-1,0,0,0);
			}
			
			p->setPen(cdata->shades[6]);
			if (below) {
				p->drawLine(tr.left(), tr.bottom() - 1,	tr.left(), tr.top() - 1);
				p->drawLine(tr.left(), tr.bottom(),	tr.right(), tr.bottom());
				p->drawLine(tr.right(), tr.top(), tr.right(), tr.bottom() - 1);
			} else {
				p->drawLine(tr.left(), tr.bottom() + 1, tr.left(), tr.top() + 1);
				p->drawLine(tr.left(), tr.top(), tr.right(), tr.top());
				p->drawLine(tr.right(), tr.top() + 1, tr.right(), tr.bottom());
			}

			p->setPen(opt->palette.light().color());
			if (below) {
				p->drawLine(tr.left() + 1, tr.bottom() - 1,	tr.left() + 1, tr.top() - 2);
				p->drawPoint(tr.right(), tr.top() - 1);
			} else {
				p->drawLine(tr.left() + 1, tr.bottom() + 2,	tr.left() + 1, tr.top() + 2);
				if (tr.left() != 0)
					p->drawPoint(tr.left(), tr.bottom() + 1);

				p->drawLine(tr.left() + 1, tr.top() + 1, tr.right() - 2, tr.top() + 1);
			}

			p->setPen(cdata->shades[2]);
			if (below) {
				if (tr.left() != 0)
					p->drawPoint(tr.left(), tr.top() - 1);
				p->drawLine(tr.left() + 2, tr.bottom() - 1,	tr.right() - 1, tr.bottom() - 1);
				p->drawLine(tr.right() - 1, tr.top() - 1, tr.right() - 1, tr.bottom() - 2);
			} else {
				p->drawLine(tr.right() - 1, tr.top() + 1, tr.right() - 1, tr.bottom() + 1);
			}
		}		

		p->fillRect(fr, ((opt->state & State_Selected) ? opt->palette.window().color() : opt->palette.dark().color()));
		break;
	}

	case CE_TabBarTabLabel: {
		const QStyleOptionTab *tabOpt = qstyleoption_cast<const QStyleOptionTab *>(opt);
		
		QRect tr = r;

		int alignment = Qt::AlignLeft | Qt::TextShowMnemonic;

		// Qt3 stylehint relied only on the style hint itself, so use nullptr for the other arguments
		if (!styleHint(SH_UnderlineShortcut, nullptr, nullptr, nullptr))
			alignment |= Qt::TextHideMnemonic;

		// Move the text bounding rectangle to place the text correctly
		tr.translate(5, 2);
		drawItemText(p, tr, alignment, opt->palette, opt->state & State_Enabled, tabOpt->text, QPalette::ButtonText);
		// Now move it back for the focus rectangle (yes, it's a a hack)
		tr.translate(-2, -2);		
		break;
	}

	case CE_MenuItem: {
		const QStyleOptionMenuItem *miOpt = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
		if (!widget)
			break;

		const QMenu* menu = static_cast<const QMenu*>(widget);
		int tab = miOpt->reservedShortcutWidth;
		int maxpmw = miOpt->maxIconWidth;

		if ( miOpt && miOpt->menuItemType == QStyleOptionMenuItem::Separator ) {
			// draw separator
			p->fillRect(r, opt->palette.brush(QPalette::Button));
			p->setPen(cdata->shades[2]);
			p->drawLine(r.left() + 6, r.top() + 4, r.right() - 6, r.top() + 4);
			p->setPen(opt->palette.light().color());
			p->drawLine(r.left() + 6,  r.top() + 5, r.right() - 6, r.top() + 5);
			break;
		}

		if ((opt->state & State_Selected) && (opt->state & State_Enabled)) {
			drawGradientBox(p, r, opt->palette, cdata, false, 0.9, 1.2);
		} else {
			p->fillRect(r, opt->palette.brush(QPalette::Button));
		}

		if (!miOpt)
			break;

		maxpmw = std::max(maxpmw, 22);

		QRect cr, ir, tr, sr;
		// check column
		cr.setRect(r.left(), r.top(), maxpmw, r.height());
		// submenu indicator column
		sr.setCoords(r.right() - 12, r.top(), r.right(), r.bottom());
		// tab/accelerator column
		tr.setCoords(r.right() - tab - 4, r.top(), r.right() - 4, r.bottom());
		// item column
		ir.setCoords(cr.right() + 4, r.top(), tr.right() - 4, r.bottom());

		bool reverse = QGuiApplication::isRightToLeft();
		if ( reverse ) {
			cr = visualRect( opt->direction, r, cr );
			sr = visualRect( opt->direction, r, sr );
			tr = visualRect( opt->direction, r, tr );
			ir = visualRect( opt->direction, r, ir );
		}

		if (!miOpt->icon.isNull()) {
			QIcon::Mode mode =
				(opt->state & State_Enabled) ? QIcon::Normal : QIcon::Disabled;
			if ((opt->state & State_Selected) && (opt->state & State_Enabled))
				mode = QIcon::Active;
			QPixmap pixmap;
			if (miOpt->menuHasCheckableItems && miOpt->checked)
			    pixmap = miOpt->icon.pixmap(pixelMetric(PM_SmallIconSize), mode, QIcon::On);
			else
				pixmap = miOpt->icon.pixmap(pixelMetric(PM_SmallIconSize), mode);
			QRect pmr(QPoint(0, 0), pixmap.size());
			pmr.moveCenter(cr.center());
			p->setPen(opt->palette.text().color());
			p->drawPixmap(pmr.topLeft(), pixmap);
		} else if (miOpt->menuHasCheckableItems && miOpt->checked) {
			QStyleOption checkOpt;
			checkOpt.state = opt->state & (State_Enabled|State_Selected);
			checkOpt.rect = cr;
			checkOpt.palette = opt->palette;
			drawPrimitive(PE_IndicatorMenuCheckMark, &checkOpt, p, widget);
		}
		QColor textcolor;
		QColor embosscolor;
		if (opt->state & QStyle::State_Selected) {
			if (! (opt->state & State_Enabled)) {
				textcolor = opt->palette.text().color();
				embosscolor = opt->palette.light().color();
			} else {
				textcolor = opt->palette.highlightedText().color();
				embosscolor = opt->palette.midlight().color().lighter();
			}
		} else if (! (opt->state & State_Enabled)) {
			textcolor = opt->palette.text().color();
			embosscolor = opt->palette.light().color();
		} else
			textcolor = embosscolor = opt->palette.buttonText().color();			
		p->setPen(textcolor);

		// mi->custom() no longer exists in Qt6, I don't believe there to be a modern equivalent
		// but if there is one, I will add the code here, in line with the Qt3 theme.

		QString text = miOpt->text;
		if (! text.isNull()) {
			int t = (int)text.indexOf('\t');

			// draw accelerator/tab-text
			if (t >= 0) {
				int alignFlag = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
				alignFlag |= ( reverse ? Qt::AlignLeft : Qt::AlignRight );
				if (! (opt->state & State_Enabled)) {
					p->setPen(embosscolor);
					tr.translate(1, 1);
					p->drawText(tr, alignFlag, text.mid(t + 1));
					tr.translate(-1, -1);
					p->setPen(textcolor);
				}

				p->drawText(tr, alignFlag, text.mid(t + 1));
			}

			int alignFlag = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
			alignFlag |= ( reverse ? Qt::AlignLeft : Qt::AlignRight );

			if (! (opt->state & State_Enabled)) {
				p->setPen(embosscolor);
				ir.translate(1, 1);
				p->drawText(ir, alignFlag, text);
				ir.translate(-1, -1);
				p->setPen(textcolor);
			}

			p->drawText(ir, alignFlag, text);
		} // mi->pixmap() is deprecated for a long time now, so ignore it

		if (miOpt->menuItemType == QStyleOptionMenuItem::SubMenu) {
			QStyleOption arrowOpt;
			arrowOpt.state = opt->state;
			arrowOpt.rect = sr;
			arrowOpt.palette = opt->palette;
			drawPrimitive((reverse ? PE_IndicatorArrowLeft : PE_IndicatorArrowRight), &arrowOpt, p, widget);
		}

		break;
		
	}

	case CE_MenuBarEmptyArea: {
		p->fillRect(r, opt->palette.brush(QPalette::Button));
		break;
	}

	case CE_MenuBarItem: {
		if ((opt->state & State_Enabled) && (opt->state & State_Sunken))
			drawGradientBox(p, r, opt->palette, cdata, false, 0.9, 1.2);
		else
			p->fillRect(r, opt->palette.brush(QPalette::Button));

		const QStyleOptionMenuItem *miOpt = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);

		if (opt->state & State_Sunken)
			drawItemText(p, r, Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine,
						 opt->palette, opt->state & State_Enabled, miOpt->text, QPalette::HighlightedText);
		else
			drawItemText(p, r, Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine,
						 opt->palette, opt->state & State_Enabled, miOpt->text, QPalette::ButtonText);
		break;
	}

	case CE_ProgressBarGroove: {
		p->setBrush(cdata->shades[3]);
		p->setPen(cdata->shades[5]);
		p->drawRect(r.adjusted(0,0,-1,-1));
		break;
	}

	case CE_ProgressBarContents: {
		const QStyleOptionProgressBar *progressbarOpt = qstyleoption_cast<const QStyleOptionProgressBar *>(opt);
	    bool reverse = QGuiApplication::isRightToLeft();

		QRect pr;

		if ((progressbarOpt->minimum == 0) && (progressbarOpt->maximum == 0)) {
			int w, remains;

			// draw busy indicator

			w = std::min(25, r.width()/2);
			w = std::max(w, 1);

			remains = r.width() - w;
			remains = std::max(remains, 1);

			int x = progressbarOpt->progress % (remains * 2);
			if (x > remains)
				x = 2 * remains - x;

			x = reverse ? r.right() - x - w : x + r.left();
			pr.setRect (x, r.top(), w, r.height());
		} else {
			int pos = progressbarOpt->progress;
			int total = (progressbarOpt->maximum - progressbarOpt->minimum) ?
				(progressbarOpt->maximum - progressbarOpt->minimum) : 1;
			int w = (int)(((double)pos*r.width())/total);

			if (reverse)
				pr.setRect (r.right() - w, r.top(), w, r.height());
			else
				pr.setRect (r.left(), r.top(), w, r.height());
		}
		drawGradientBox(p, pr, opt->palette, cdata, false, 0.92, 1.66);
		
		break;
	}

	case CE_CheckBoxLabel: {
		const QStyleOptionButton *checkboxOpt = qstyleoption_cast<const QStyleOptionButton *>(opt);

		if (opt->state & State_MouseOver) {
			QRegion r(opt->rect);
			r -= visualRect(opt->direction, subElementRect(SE_CheckBoxIndicator, opt, widget), opt->rect);
			p->setClipRegion(r);
			p->fillRect(opt->rect, opt->palette.brush(QPalette::Midlight));
			p->setClipping(false);
		}

		int alignment = QGuiApplication::isRightToLeft() ? Qt::AlignRight : Qt::AlignLeft;
		drawItemText(p, r, alignment | Qt::AlignVCenter | Qt::TextShowMnemonic,
					 opt->palette, opt->state & State_Enabled, checkboxOpt->text);	
		
		break;
	}

	case CE_RadioButtonLabel: {
		const QStyleOptionButton *radioOpt = qstyleoption_cast<const QStyleOptionButton *>(opt);

		if (opt->state & State_MouseOver) {
			QRegion r(opt->rect);
			r -= visualRect(opt->direction, subElementRect(SE_CheckBoxIndicator, opt, widget), opt->rect);
			p->setClipRegion(r);
			p->fillRect(opt->rect, opt->palette.brush(QPalette::Midlight));
			p->setClipping(false);
		}

		int alignment = QGuiApplication::isRightToLeft() ? Qt::AlignRight : Qt::AlignLeft;
		drawItemText(p, r, alignment | Qt::AlignVCenter | Qt::TextShowMnemonic,
					 opt->palette, opt->state & State_Enabled, radioOpt->text);	
		
		break;
	}

		/* CE's that were PE's in Qt3 go below */

	case CE_HeaderSection: {
		const QBrush *fill;
		QStyle::State state = ((opt->state | State_Sunken) ^ State_Sunken) | State_Raised;
		if (! (state & State_Sunken) && (state & State_On))
			fill = &opt->palette.brush(QPalette::Midlight);
		else
			fill = &opt->palette.brush(QPalette::Button);

	    QStyleOption optCopy(*opt);
		optCopy.state = state;

		drawLightBevel(p, &optCopy, fill);
		p->setPen( opt->palette.buttonText().color() );
		
		break;
	}

	case CE_Splitter: {
		if (opt->state & State_Horizontal) {
			int y_mid = r.height()/2;
			for (int i=0; i< 21; i=i+5) {
				p->setPen(cdata->shades[5]);
				p->drawLine(r.x()+1, y_mid-10+i, r.right()-1, y_mid-10+i-3);
				p->setPen(opt->palette.light().color());
				p->drawLine(r.x()+1, y_mid-10+i+1, r.right()-1, y_mid-10+i-2);
			}
		} else {
			int x_mid = r.width()/2;
			for (int i=0; i< 21; i=i+5) {
				p->setPen(cdata->shades[5]);
				p->drawLine(x_mid-10+i+3, r.y()+1, x_mid-10+i, r.bottom()-1);
				p->setPen(opt->palette.light().color());
				p->drawLine(x_mid-10+i+4, r.y()+1, x_mid-10+i+1, r.bottom()-1);
			}
		}
	}

	case CE_ScrollBarAddLine:
	case CE_ScrollBarSubLine: {
		// Highlight on mouse over
		QStyleOption optCopy(*opt);
	    optCopy.state = opt->state | ((opt->state & State_Enabled) ? State_Raised : QStyle::State());
		drawLightBevel(p, &optCopy, &opt->palette.brush((opt->state & State_MouseOver) ? QPalette::Midlight : QPalette::Button), true);

		PrimitiveElement pe;

		if ((control == CE_ScrollBarAddLine) && (opt->state & State_Horizontal)) {
			pe = PE_IndicatorArrowRight;
		} else if (control == CE_ScrollBarAddLine) {
			pe = PE_IndicatorArrowDown;
		} else if (opt->state & State_Horizontal) {
			pe = PE_IndicatorArrowLeft;
		} else {
			pe = PE_IndicatorArrowUp;
		}

		QStyleOption arrowOpt(*opt);
		arrowOpt.state = opt->state & ~State_MouseOver;
	    drawPrimitive(pe, &arrowOpt, p, widget);
	  
		break;
	}

	case CE_ScrollBarSubPage:
	case CE_ScrollBarAddPage: {
		p->fillRect(r, cdata->shades[3]);
		p->setPen(cdata->shades[5]);
		if (opt->state & State_Horizontal) {
			p->drawLine(r.left(), r.top(), r.right(), r.top());
			p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
		} else {
			p->drawLine(r.left(), r.top(), r.left(), r.bottom());
			p->drawLine(r.right(), r.top(), r.right(), r.bottom());
		}
		break;
	}

	case CE_ScrollBarSlider: {
		int x1, y1;
		p->setPen(opt->palette.dark().color());
		// Not the best place to put this, but it seems to be the only place where it works correctly

		// Expand the slider one pixel in each direction, 
		// to avoid double lines in the extreme positions.
		QRect r2 = r;
		if (opt->state & State_Horizontal) {
			r2.setWidth(r.width() + 1);
			r2.setX(r.x() - 1);
		} else {
			r2.setHeight(r.height() + 1);
			r2.setY(r.y() - 1);
		}

		// Highlight on mouse over
		QStyleOption optCopy(*opt);
	    optCopy.state = (opt->state & ~State_Sunken) | ((opt->state & State_Enabled) ? State_Raised : QStyle::State());
	    drawLightBevel(p, &optCopy, &opt->palette.brush((opt->state & State_MouseOver) ? QPalette::Midlight : QPalette::Button), true);

		if (opt->state & State_Horizontal && r.width() < 31)
			break;
		if (!(opt->state & State_Horizontal) && r.height() < 31)
			break;

		// Scrollbar diagonal handle
		p->setPen(cdata->shades[5]);
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
	QRect rect, wrect(opt->rect);

	switch (element) {
	case SE_PushButtonFocusRect: {
		const QStyleOptionButton *buttonOpt = qstyleoption_cast<const QStyleOptionButton *>(opt);
		int dbw1 = 0, dbw2 = 0;
		if ((buttonOpt->features & QStyleOptionButton::DefaultButton) ||
			(buttonOpt->features & QStyleOptionButton::AutoDefaultButton)) {
			dbw1 = pixelMetric(PM_ButtonDefaultIndicator, opt, widget);
			dbw2 = dbw1 * 2;
		}

		rect.setRect(wrect.x()	  + 3 + dbw1,
					 wrect.y()	  + 3 + dbw1,
					 wrect.width()  - 6 - dbw2,
					 wrect.height() - 6 - dbw2);
		break;
	}

	case SE_CheckBoxIndicator: {
		int h = pixelMetric( PM_IndicatorHeight );
		rect.setRect(( opt->rect.height() - h ) / 2,
					 ( opt->rect.height() - h ) / 2,
					 pixelMetric( PM_IndicatorWidth ), h );
		break;
	}

	case SE_RadioButtonIndicator: {
		int h = pixelMetric( PM_ExclusiveIndicatorHeight );
		rect.setRect( ( opt->rect.height() - h ) / 2,
					  ( opt->rect.height() - h ) / 2,
					  pixelMetric( PM_ExclusiveIndicatorWidth ), h );
		break;
	}
		
	default: {
		rect = QCommonStyle::subElementRect(element, opt, widget);
		break;
	}
	}

	return rect;
}

void
BluecurveStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *opt,
								   QPainter *p, const QWidget *widget) const
{
	const BluecurveColorData *cdata = lookupData(opt->palette);
	
	switch (control) {
	case CC_ComboBox: {
		const QStyleOptionComboBox *comboboxOpt = qstyleoption_cast<const QStyleOptionComboBox *>(opt);
		QRect frame, arrow, field;

		frame = visualRect(opt->direction, subControlRect(CC_ComboBox, opt, SC_ComboBoxFrame, widget), opt->rect);
		arrow = visualRect(opt->direction, subControlRect(CC_ComboBox, opt, SC_ComboBoxArrow, widget), opt->rect);
		field = visualRect(opt->direction, subControlRect(CC_ComboBox, opt, SC_ComboBoxEditField, widget), opt->rect);

		if ((opt->subControls & SC_ComboBoxFrame) && frame.isValid()) {
			QStyleOption frameOpt;
			frameOpt.state = opt->state | State_Raised;
			frameOpt.rect = frame;
			frameOpt.palette = opt->palette;
			if (opt->state & State_MouseOver)
				drawLightBevel(p, &frameOpt, &opt->palette.brush(QPalette::Midlight), true);
			else
				drawLightBevel(p, &frameOpt, &opt->palette.brush(QPalette::Button), true);
		}

		if ((opt->subControls & SC_ComboBoxArrow) && arrow.isValid()) {
			QStyleOption arrowOpt;
			arrowOpt.state = opt->state & ~State_MouseOver;
			arrowOpt.rect = arrow;
			arrowOpt.palette = opt->palette;
			
			drawPrimitive(PE_IndicatorArrowDown, &arrowOpt, p);
			p->setPen(cdata->shades[3]);
			p->drawLine((arrow.left()+arrow.right())/2-2, 
						(arrow.top()+arrow.bottom())/2+5, 
						(arrow.left()+arrow.right())/2+2,
						(arrow.top()+arrow.bottom())/2+5);
			p->drawLine((arrow.left()+arrow.right())/2-2,
						(arrow.top()+arrow.bottom())/2+6,
						(arrow.left()+arrow.right())/2+2,
						(arrow.top()+arrow.bottom())/2+6);
		}

		if ((opt->subControls & SC_ComboBoxEditField) && field.isValid()) {
			p->setPen(cdata->shades[4]);
			if (comboboxOpt->editable) {
				field.adjust(-1, -1, 1, 1);
				p->drawLine(field.right(), field.top() - 1,
							field.right(), field.bottom() + 1);
				p->setPen(opt->palette.light().color());
				p->drawLine(field.right() + 1, field.top(),
							field.right() + 1, field.bottom() );
			} else {
				p->drawLine(field.right() + 1, field.top() - 2,
							field.right() + 1, field.bottom() + 2);
				p->setPen(opt->palette.light().color());
				p->drawLine(field.right() + 2, field.top() - 1,
							field.right() + 2, field.bottom() + 1);
			}

			if (opt->state & State_HasFocus) {
				if (! comboboxOpt->editable) {
					QRect fr = visualRect(opt->direction, subElementRect(SE_ComboBoxFocusRect, opt, widget), opt->rect);
					fr.setRight(fr.right() - 3);
					QStyleOption focusRectOpt;
					focusRectOpt.state = opt->state | State_FocusAtBorder;
					focusRectOpt.rect = fr;
					focusRectOpt.palette = opt->palette;
					drawPrimitive(PE_FrameFocusRect, opt, p);
				}
			}

			if (opt->state & State_Enabled)
				p->setPen(opt->palette.buttonText().color());
			else
				p->setPen(opt->palette.mid().color());
			
		}
		
		break;
	}

	case CC_SpinBox: {
		const QStyleOptionSpinBox *spinboxOpt = qstyleoption_cast<const QStyleOptionSpinBox *>(opt);
		QRect frame, up, down;

		frame = subControlRect(CC_SpinBox, opt, SC_SpinBoxFrame, widget);
		up = subControlRect(CC_SpinBox, opt, SC_SpinBoxUp, widget);
		down = subControlRect(CC_SpinBox, opt, SC_SpinBoxDown, widget);

		if ((opt->subControls & SC_SpinBoxFrame) && frame.isValid()) {
			QStyleOption textOpt;
			textOpt.state = opt->state | State_Sunken;
			textOpt.rect = frame;
			textOpt.palette = opt->palette;
			drawTextRect(p, &textOpt, &opt->palette.brush(QPalette::Base));
		}

		p->setPen(cdata->shades[5]);
		p->drawLine(up.topLeft(), down.bottomLeft());
		p->drawLine(up.left(), up.bottom()+1, up.right(), up.bottom()+1);

		if ((opt->subControls & SC_SpinBoxUp) && up.isValid()) {
			PrimitiveElement pe = PE_IndicatorSpinUp;

			if (spinboxOpt->buttonSymbols == QAbstractSpinBox::PlusMinus)
				pe = PE_IndicatorSpinPlus;

			up.adjust(1, 0, 0, 0);
			p->fillRect(up, opt->palette.brush(QPalette::Button));

			if (opt->activeSubControls == SC_SpinBoxUp)
				p->setPen(cdata->shades[2]);
			else
				p->setPen(opt->palette.light().color());

			p->drawLine(up.left(), up.top(), up.right(), up.top());
			p->drawLine(up.left(), up.top(), up.left(), up.bottom());

			if (opt->activeSubControls == SC_SpinBoxUp)
				p->setPen(opt->palette.light().color());
			else
				p->setPen(cdata->shades[2]);

			p->drawLine(up.right(), up.top()+1, up.right(), up.bottom());
			p->drawLine(up.left()+1, up.bottom(), up.right(), up.bottom());

			up.adjust(1, 0, 0, 0);

			QStyleOption indicatorOpt;
			indicatorOpt.state =  opt->state | ((opt->activeSubControls == SC_SpinBoxUp) ? State_On | State_Sunken : State_Raised);
			indicatorOpt.rect = up;
			indicatorOpt.palette = opt->palette;
			drawPrimitive(pe, opt, p);
		}

		if ((opt->subControls & SC_SpinBoxDown) && down.isValid()) {
			PrimitiveElement pe = PE_IndicatorSpinDown;

			if (spinboxOpt->buttonSymbols == QAbstractSpinBox::PlusMinus)
				pe = PE_IndicatorSpinMinus;

			down.adjust(1, 0, 0, 0);
			p->fillRect(down, opt->palette.brush(QPalette::Button));

			if (opt->activeSubControls == SC_SpinBoxDown)
				p->setPen(cdata->shades[2]);
			else
				p->setPen(opt->palette.light().color());

			p->drawLine(down.left(), down.top(), down.right(), down.top());
			p->drawLine(down.left(), down.top(), down.left(), down.bottom());

			if (opt->activeSubControls == SC_SpinBoxDown)
				p->setPen(opt->palette.light().color());
			else
				p->setPen(cdata->shades[2]);

			p->drawLine(down.right(), down.top()+1,
						down.right(), down.bottom());
			p->drawLine(down.left()+1, down.bottom(),
						down.right(), down.bottom());

			down.adjust(1, 0, 0, 0);

			QStyleOption indicatorOpt;
			indicatorOpt.state =  opt->state | ((opt->activeSubControls == SC_SpinBoxDown) ? State_On | State_Sunken : State_Raised);
			indicatorOpt.rect = down;
			indicatorOpt.palette = opt->palette;
			drawPrimitive(pe, opt, p);
		}
		
		break;
	}

	case CC_Slider: {
		const QStyleOptionSlider *sliderOpt = qstyleoption_cast<const QStyleOptionSlider *>(opt);
		QRect groove = subControlRect(CC_Slider, opt, SC_SliderGroove, widget);
		QRect handle = subControlRect(CC_Slider, opt, SC_SliderHandle, widget);

		if ((opt->subControls & SC_SliderGroove) && groove.isValid()) {
			if (opt->state & State_HasFocus) {
				QStyleOption focusOpt;
				focusOpt.state = QStyle::State();
				focusOpt.rect = groove;
				focusOpt.palette = opt->palette;
				drawPrimitive(PE_FrameFocusRect, &focusOpt, p);
			}

			if (sliderOpt->orientation == Qt::Horizontal) {
				int dh = (groove.height() - 5) / 2;
				groove.adjust(0, dh, 0, -dh);
				handle.adjust(0, 1, 0, -1);
			} else {
				int dw = (groove.width() - 5) / 2;
				groove.adjust(dw, 0, -dw, 0);
				handle.adjust(1, 0, -1, 0);
			}

			p->setPen(cdata->shades[5]);
			p->setBrush(opt->palette.mid().color());
			p->drawRect(groove.adjusted(0,0,-1,-1));
			p->setPen(cdata->shades[4]);
			p->drawLine(groove.left()+1, groove.top()+1,
						groove.left()+1, groove.bottom()-1);
			p->drawLine(groove.left()+1, groove.top()+1,
						groove.right()-1, groove.top()+1);
		}

		if ((opt->subControls & SC_SliderHandle) && handle.isValid()) {
			p->setPen(cdata->shades[6]);
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
			  

			p->setPen(cdata->shades[2]);
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

			p->setBrush(opt->palette.button().color());
			QRect fillr (handle);
			fillr.adjust(2, 2, -2, -2);
			p->fillRect (fillr, opt->palette.brush(QPalette::Button));

			if (sliderOpt->orientation == Qt::Horizontal) {
				int x1 = handle.x() + handle.width() / 2 - 8;
				int y1 = handle.y() + (handle.height() - 6) / 2;
				p->setPen(cdata->shades[5]);
				p->drawLine(x1 + 5, y1,
							x1, y1 + 5);
				p->drawLine(x1 + 5 + 5, y1,
							x1 + 5, y1 + 5);
				p->drawLine(x1 + 5 + 5*2, y1,
							x1 + 5*2, y1 + 5);
				
				p->setPen(Qt::white);
				p->drawLine(x1 + 5, y1 + 1,
							x1 + 1, y1 + 5);
				p->drawLine(x1 + 5 + 5, y1 + 2,
							x1 + 5 + 1, y1 + 5);
				p->drawLine(x1 + 5 + 5*2, y1 + 1,
							x1 + 5*2 + 1, y1 + 5);
			} else {
				int x1 = handle.x() + (handle.width() - 6) / 2;
				int y1 = handle.y() + handle.height() / 2 - 8;
				p->setPen(cdata->shades[5]);
				p->drawLine(x1 + 5, y1,
							x1, y1 + 5);
				p->drawLine(x1 + 5, y1 + 5,
							x1, y1 + 5 + 5);
				p->drawLine(x1 + 5, y1 + 5*2,
							x1, y1 + 5 + 5*2);
				p->setPen(Qt::white);
				p->drawLine(x1 + 5, y1 + 1,
							x1 + 1, y1 + 5);
				p->drawLine(x1 + 5, y1 + 5 + 1,
							x1 + 1, y1 + 5 + 5);
				p->drawLine(x1 + 5, y1 + 5*2 + 1,
							x1 + 1, y1 + 5 + 5*2);
			}
		}

		if (opt->subControls & SC_SliderTickmarks) {
			QStyleOptionComplex optCopy(*opt);
			optCopy.subControls = SC_SliderTickmarks;
			QCommonStyle::drawComplexControl(control, &optCopy, p, widget);
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
	case CC_SpinBox: {
		int fw = pixelMetric( PM_SpinBoxFrameWidth, opt, widget);
		QSize bs;
		bs.setHeight( widget->height()/2 - fw );
		if ( bs.height() < 8 )
			bs.setHeight( 8 );
		bs.setWidth( bs.height() * 8 / 6 ); 
		//bs = bs.expandedTo( QApplication::globalStrut() ); no qt6 equivalent 
		int y = fw;
		int x, lx, rx;
		x = widget->width() - y - bs.width() + 1;
		lx = fw;
		rx = x - fw;
		switch ( sc ) {
		case SC_SpinBoxUp: {
			ret.setRect(x, y-1, bs.width(), bs.height()+1);
			break;
		}
		case SC_SpinBoxDown: {
			ret.setRect(x, y + bs.height()+1, bs.width(), bs.height()+1);
			break;
		}
		case SC_SpinBoxEditField: {
			ret.setRect(lx, fw, rx, widget->height() - 2*fw);
			break;
		}
		case SC_SpinBoxFrame: {
			ret = widget->rect();
		}
		default: {
			break;
		}
		}
		
		break;
	}

	case CC_ComboBox: {
		ret = QCommonStyle::subControlRect(control, opt, sc, widget);
		switch (sc) {
		case SC_ComboBoxFrame: {
			break;
		}
		case SC_ComboBoxArrow: {
			ret.setTop(ret.top() - 2);
			ret.setLeft(ret.left() - 1);
			break;
		}
		case SC_ComboBoxEditField: {
			ret.setRight(ret.right() - 2);
			break;
		}
		default: {
			break;
		}
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
	int ret;
	
	switch (metric) {
	case PM_ButtonMargin: {
		ret = 10;
		break;
	}

	case PM_ButtonShiftHorizontal:
	case PM_ButtonShiftVertical: {
		ret = 0;
		break;
	}

	case PM_ButtonDefaultIndicator: {
		ret = 0;
		break;
	}

	case PM_ButtonIconSize: {
		ret = 20;
		break;
	}

	case PM_DefaultFrameWidth: {
		if (widget && widget->inherits("QMenu")) {
			ret = 3;
		} else if (widget && widget->inherits("QStackedWidget")) {
			ret = 2;
		} else {
			ret = 1;
		}
		break;
	}

	case PM_IndicatorWidth:
	case PM_IndicatorHeight:
	case PM_ExclusiveIndicatorWidth:
	case PM_ExclusiveIndicatorHeight: {
		ret = 13;
		break;
	}

	case PM_MenuVMargin: {
		ret = 1;
		break;
	}

		/*case PM_TabBarTabOverlap: { seems to have no effect in Qt6
		ret = 1;
		break;
		}*/

	case PM_TabBarTabHSpace: {
		ret = 10;
		break;
	}

	case PM_TabBarTabVSpace: {
		ret = 10;
		break;
	}

	case PM_TabBarBaseOverlap: {
		ret = 2;
		break;
	}

	case PM_ScrollBarExtent: {
		ret = 15;
		break;
	}

	case PM_MenuBarPanelWidth: {
		ret = 1;
		break;
	}

	case PM_ProgressBarChunkWidth: {
		ret = 2;
		break;
	}

	case PM_DockWidgetSeparatorExtent: {
		ret = 4;
		break;
	}

	case PM_DockWidgetHandleExtent: {
		ret = 10;
		break;
	}

	case PM_SplitterWidth: {
		ret = 6;
		break;
	}

	case PM_ScrollBarSliderMin: {
		ret=31;
		break;
	}

	case PM_SliderControlThickness: {
		ret = basestyle->pixelMetric( metric, opt, widget );
		break;
	}

	case PM_SliderLength: {
		ret=31;
		if (widget->inherits("QSlider")) {
			const QSlider *slider = static_cast<const QSlider*>(widget);
			if (slider->orientation() == Qt::Horizontal) {
				if (widget->width()<ret)
					ret = widget->width();
			} else {
				if (widget->height()<ret)
					ret = widget->height();
			}
		}
		break;
	}

	case PM_MaximumDragDistance: {
		ret = -1;
		break;
	}
		
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

	case CT_MenuItem: {
		const QStyleOptionMenuItem *miOpt = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
		int maxpmw = miOpt->maxIconWidth;
		int w = contentsSize.width(), h = contentsSize.height();

		if (miOpt->menuItemType == QStyleOptionMenuItem::Separator) {
			w = 10;
			h = 12;
		} else {
			// check is at least 16x16
			if (h < 16) {
				h = 16;
			}

			if (! miOpt->icon.isNull()) {
				h = std::max(h, pixelMetric(PM_SmallIconSize) + 6);
			} else if (! miOpt->text.isNull()) {
				h = std::max(h, opt->fontMetrics.height() + 8);
			}
		}

		// check is at least 16x16
		maxpmw = std::max(maxpmw, 16);
		w += maxpmw + 16;

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
	case SH_Slider_SnapToValue:
	case SH_PrintDialog_RightAlignButtons:
	case SH_FontDialog_SelectAssociatedText:
	case SH_MenuBar_AltKeyNavigation:
	case SH_MenuBar_MouseTracking:
	case SH_Menu_MouseTracking:
	case SH_Menu_SpaceActivatesItem:
	case SH_ComboBox_ListMouseTracking:
	case SH_ScrollBar_MiddleClickAbsolutePosition: {
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
BluecurveStyle::drawGradientBox(QPainter *p, QRect const &r,
								const QPalette &palette,
								const BluecurveColorData *cdata,
								bool horiz,
								double shade1, double shade2) const
{
	QRect grad(r.left()+2, r.top()+2, r.width()-3, r.height()-3);
	drawGradient(p, grad, palette, shade1, shade2, horiz);

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
}
