//========================================================================
//
// ImgOutputDev.cc
//
// Copyright 2011 Devaldi Ltd
//
// Copyright 1997-2002 Glyph & Cog, LLC
//
// Changed 1999-2000 by G.Ovtcharov
//
// Changed 2002 by Mikhail Kruk
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include <math.h>
#include "GString.h"
#include "GList.h"
#include "UnicodeMap.h"
#include "gmem.h"
#include "config.h"
#include "Error.h"
#include "GfxState.h"
#include "GlobalParams.h"
#include "ImgOutputDev.h"
#include "XmlFonts.h"


int HtmlPage::pgNum=0;
int ImgOutputDev::imgNum=1;

extern double scale;
extern GBool complexMode;
extern GBool ignore;
extern GBool printCommands;
extern GBool printHtml;
extern GBool noframes;
extern GBool stout;
extern GBool xml;
extern GBool showHidden;
extern GBool noMerge;

static GString* basename(GString* str) {
	char *p=str->getCString();
	int len=str->getLength();
	for (int i=len-1;i>=0;i--) {
		if (*(p+i)==SLASH) {
			return new GString((p+i+1),len-i-1);
		}
	}
	return new GString(str);
}

//------------------------------------------------------------------------
// HtmlString
//------------------------------------------------------------------------

HtmlString::HtmlString(GfxState *state, double fontSize, double _charspace, XmlFontAccu* fonts) {

	GfxFont *font;
	double x, y;

	state->transform(state->getCurX(), state->getCurY(), &x, &y);

	if ((font = state->getFont())) {
		yMin = y - font->getAscent() * fontSize;
		yMax = y - font->getDescent() * fontSize;
		GfxRGB rgb;
		state->getFillRGB(&rgb);
		GString *name = state->getFont()->getName();
		
		if (!name) name = XmlFont::getDefaultFont();

		XmlFont hfont=XmlFont(name, static_cast<int>(fontSize-1),0.0, rgb);
		fontpos = fonts->AddFont(hfont);

	} else {

		yMin = y - 0.95 * fontSize;
		yMax = y + 0.35 * fontSize;
		fontpos=0;

	}
	
	if (yMin == yMax) {
		yMin = y;
		yMax = y + 1;
	}

	col = 0;
	text = NULL;
	xRight = NULL;
	link = NULL;
	len = size = 0;
	yxNext = NULL;
	xyNext = NULL;
	strSize = 0;
	htext=new GString();
	htext2=new GString();
	dir = textDirUnknown;
}


HtmlString::~HtmlString() {
	delete text;
	delete htext;
	delete htext2;
	gfree(xRight);
}

void HtmlString::addChar(GfxState *state, double x, double y, double dx, double dy, Unicode u) {

	if ( !showHidden && (state->getRender() & 3) == 3) return;

	if (dir == textDirUnknown) dir = UnicodeMap::getDirection(u);

	if (len == size) {
		size += 16;
		text = (Unicode *)grealloc(text, size * sizeof(Unicode));
		xRight = (double *)grealloc(xRight, size * sizeof(double));
	}

	text[len] = u;

	if (len == 0) xMin = x;

	xMax = xRight[len] = x + dx;
	++strSize;
	++len;

}

void HtmlString::endString() {
	if (dir == textDirRightLeft && len > 1) {
		for (int i = 0; i < len / 2; i++) {
			Unicode ch = text[i];
			text[i] = text[len - i - 1];
			text[len - i - 1] = ch;
		}
	}
}

//------------------------------------------------------------------------
// HtmlPage
//------------------------------------------------------------------------

HtmlPage::HtmlPage(GBool rawOrder, GBool textAsJSON, GBool compressData, char *imgExtVal) {

	this->rawOrder = rawOrder;
	this->textAsJSON = textAsJSON;
	this->compressData = compressData;

	curStr = NULL;
	yxStrings = NULL;
	xyStrings = NULL;
	yxCur1 = yxCur2 = NULL;
	fonts=new XmlFontAccu();
	links=new XmlLinks();
	pageWidth=0;
	pageHeight=0;
	X1=0;
	X2=0;  
	Y1=0;
	Y2=0;  
	fontsPageMarker = 0;
	DocName=NULL;
	firstPage = -1;
	imgExt = new GString(imgExtVal);

}

HtmlPage::~HtmlPage() {
	clear();
	if (DocName) delete DocName;
	if (fonts) delete fonts;
	if (links) delete links;
	if (imgExt) delete imgExt;  
}

void HtmlPage::updateFont(GfxState *state) {

	GfxFont *font;
	double *fm;
	char *name;
	int code;
	double w;

	fontSize = state->getTransformedFontSize();

	if ((font = state->getFont()) && font->getType() == fontType3) {
		// This is a hack which makes it possible to deal with some Type 3
		// fonts.  The problem is that it's impossible to know what the
		// base coordinate system used in the font is without actually
		// rendering the font.  This code tries to guess by looking at the
		// width of the character 'm' (which breaks if the font is a
		// subset that doesn't contain 'm').
		for (code = 0; code < 256; ++code) {
			if ((name = ((Gfx8BitFont *)font)->getCharName(code)) && name[0] == 'm' && name[1] == '\0') {
				break;
			}
		}
		if (code < 256) {
			w = ((Gfx8BitFont *)font)->getWidth(code);
			if (w != 0) {
				// 600 is a generic average 'm' width -- yes, this is a hack
				fontSize *= w / 0.6;
			}
		}
		fm = font->getFontMatrix();
		if (fm[0] != 0) {
			fontSize *= fabs(fm[3] / fm[0]);
		}
	}
}

void HtmlPage::beginString(GfxState *state, GString *s) {
	curStr = new HtmlString(state, fontSize,charspace, fonts);
}


void HtmlPage::conv(){

	HtmlString *tmp;

	int linkIndex = 0;
	XmlFont* h;

	for(tmp=yxStrings;tmp;tmp=tmp->yxNext){
		int pos=tmp->fontpos;
		h=fonts->Get(pos);

		if (tmp->htext) delete tmp->htext; 
		tmp->htext=XmlFont::simple(h,tmp->text,tmp->len);
		tmp->htext2=XmlFont::simple(h,tmp->text,tmp->len);

		if (links->inLink(tmp->xMin,tmp->yMin,tmp->xMax,tmp->yMax, linkIndex)){
			tmp->link = links->getLink(linkIndex);
		}
	}
}

void HtmlPage::addChar(GfxState *state, double x, double y, double dx, double dy, double ox, double oy, Unicode *u, int uLen) {

	if ( !showHidden && (state->getRender() & 3) == 3) return;

	double x1, y1, w1, h1, dx2, dy2;
	int n, i, d;

	state->transform(x, y, &x1, &y1);

	n = curStr->len;
	d = 0;

	state->textTransformDelta(state->getCharSpace() * state->getHorizScaling(), 0, &dx2, &dy2);

	dx -= dx2;
	dy -= dy2;

	state->transformDelta(dx, dy, &w1, &h1);

	if (uLen != 0) {
		w1 /= uLen;
		h1 /= uLen;
	}

	for (i = 0; i < uLen; ++i) {
		if (u[i] == ' ') {
			curStr->addChar(state, x1 + i*w1, y1 + i*h1, w1, h1, u[i]);
			endString();
			beginString(state, NULL);
		} else {
			curStr->addChar(state, x1 + i*w1, y1 + i*h1, w1, h1, u[i]); /* xyString  */
		}
	}
}

void HtmlPage::endString() {
	HtmlString *p1, *p2;
	double h, y1, y2;

	// throw away zero-length strings -- they don't have valid xMin/xMax
	// values, and they're useless anyway

	if (curStr->len == 0) {
		delete curStr;
		curStr = NULL;
		return;
	}

	curStr->endString();

	// insert string in y-major list
	h = curStr->yMax - curStr->yMin;
	y1 = curStr->yMin + 0.5 * h;
	y2 = curStr->yMin + 0.8 * h;

	if (rawOrder) {
		p1 = yxCur1;
		p2 = NULL;
	} else if ((!yxCur1 || (y1 >= yxCur1->yMin && (y2 >= yxCur1->yMax || curStr->xMax >= yxCur1->xMin))) && (!yxCur2 || (y1 < yxCur2->yMin || (y2 < yxCur2->yMax && curStr->xMax < yxCur2->xMin)))) {
		p1 = yxCur1;
		p2 = yxCur2;
	} else {
		for (p1 = NULL, p2 = yxStrings; p2; p1 = p2, p2 = p2->yxNext) {
			if (y1 < p2->yMin || (y2 < p2->yMax && curStr->xMax < p2->xMin)) break;
		}
		yxCur2 = p2;
	}

	yxCur1 = curStr;

	if (p1) {
		p1->yxNext = curStr;
	} else {
		yxStrings = curStr;
	}

	curStr->yxNext = p2;
	curStr = NULL;
}

void HtmlPage::coalesce() {

	HtmlString *str1, *str2;
	XmlFont *hfont1, *hfont2;
	double space, horSpace, vertSpace, vertOverlap;
	GBool addSpace, addLineBreak;
	int n, i;
	double curX, curY, lastX, lastY;
	int sSize = 0;      
	double diff = 0.0;
	double pxSize = 0.0;
	double strSize = 0.0;
	double cspace = 0.0;

	str1 = yxStrings;
	if( !str1 ) return;

	hfont1 = getFont(str1);

	str1->htext2->append(str1->htext);
	
	if( str1->getLink() != NULL ) {
		GString *ls = str1->getLink()->getLinkStart();
		str1->htext->insert(0, ls);
		delete ls;
	}

	curX = str1->xMin; curY = str1->yMin;
	lastX = str1->xMin; lastY = str1->yMin;

	while (str1 && (str2 = str1->yxNext)) {

		hfont2 = getFont(str2);
		space = str1->yMax - str1->yMin;
		horSpace = str2->xMin - str1->xMax;
		addLineBreak = !noMerge && (fabs(str1->xMin - str2->xMin) < 0.4);
		vertSpace = str2->yMin - str1->yMax;

		if (str2->yMin >= str1->yMin && str2->yMin <= str1->yMax) {
			vertOverlap = str1->yMax - str2->yMin;
		} else if (str2->yMax >= str1->yMin && str2->yMax <= str1->yMax) {
			vertOverlap = str2->yMax - str1->yMin;
		} else {
			vertOverlap = 0;
		} 

		if (((((rawOrder && vertOverlap > 0.5 * space) || (!rawOrder && str2->yMin < str1->yMax)) && (horSpace > -0.5 * space && horSpace < space)) || (vertSpace >= 0 && vertSpace < 0.5 * space && addLineBreak)) && str1->dir == str2->dir && !(str2->len == 1 && str2->htext->getCString()[0] == ' ') && !(str1->htext->getCString()[str1->len-1] == ' ') && !(str1->htext->getLength() >= str1->len+1 && str1->htext->getCString()[str1->len+1] == ' ')) {

			diff = str2->xMax - str1->xMin;
			n = str1->len + str2->len;

			if ((addSpace = horSpace > 0.1 * space)) {
   			++n;
			}

			str1->size = (n + 15) & ~15;
			str1->text = (Unicode *)grealloc(str1->text, str1->size * sizeof(Unicode));   
			str1->xRight = (double *)grealloc(str1->xRight, str1->size * sizeof(double));

			if (addSpace) {
				str1->text[str1->len] = 0x20;
				str1->htext->append(" ");
				str1->htext2->append(" ");
				str1->xRight[str1->len] = str2->xMin;
				++str1->len;
				++str1->strSize;

				str1->xMin = curX; str1->yMin = curY; 
				str1 = str2;
				curX = str1->xMin; curY = str1->yMin;
				hfont1 = hfont2;

				if (str1->getLink() != NULL) {
					GString *ls = str1->getLink()->getLinkStart();
					str1->htext->insert(0, ls);
					delete ls;
				}

			} else {

				str1->htext2->append(str2->htext2);

				XmlLink *hlink1 = str1->getLink();
				XmlLink *hlink2 = str2->getLink();

				for (i = 0; i < str2->len; ++i) {
					str1->text[str1->len] = str2->text[i];
					str1->xRight[str1->len] = str2->xRight[i];
					++str1->len;
				}

				if( !hlink1 || !hlink2 || !hlink1->isEqualDest(*hlink2) ) {

					if(hlink2 != NULL ) {
						GString *ls = hlink2->getLinkStart();
						str1->htext->append(ls);
						delete ls;
					}
				}

				str1->htext->append(str2->htext);
				sSize = str1->htext2->getLength();      
				pxSize = xoutRoundLower(hfont1->getSize()/scale);
				strSize = (pxSize*(sSize-2));   
				cspace = (diff / strSize);
				str1->link = str2->link; 
				hfont1 = getFont(str1);
				hfont2 = getFont(str2); 

				hfont1 = hfont2;

				if (str2->xMax > str1->xMax) {
					str1->xMax = str2->xMax;
				}

				if (str2->yMax > str1->yMax) {
					str1->yMax = str2->yMax;
				}

				str1->yxNext = str2->yxNext;

				delete str2;
			}

		} else { 

			str1->xMin = curX; str1->yMin = curY; 
			str1 = str2;
			curX = str1->xMin; curY = str1->yMin;
			hfont1 = hfont2;

			if( str1->getLink() != NULL ) {
				GString *ls = str1->getLink()->getLinkStart();
				str1->htext->insert(0, ls);
				delete ls;
			}
		}
	}

	str1->xMin = curX; str1->yMin = curY;

}


void HtmlPage::dumpAsXML(FILE* f,int page, GBool passedFirstPage, int totalPages){  

	printf("");

	if (passedFirstPage) fprintf(f, ",");

	fprintf(f, "{\"number\":%d,\"pages\":%d,\"height\":%d,\"width\":%d,", page, totalPages, pageHeight, pageWidth);

	GBool passedFirst = false;

	// output fonts
	fprintf(f,"\"fonts\":[");
	for (int i=fontsPageMarker;i < fonts->size();i++) {
		GString *fontCSStyle = fonts->CSStyle(i,textAsJSON);
		if (passedFirst) fprintf(f,",");
		fprintf(f,"%s",fontCSStyle->getCString());
		passedFirst = true;
		delete fontCSStyle;
	}

	fprintf(f,"]");

	GString *str, *str1;

	passedFirst = false;

	fprintf(f,",\"text\":[");

	for (HtmlString *tmp=yxStrings; tmp; tmp=tmp->yxNext) {

		if (tmp->htext){

			str=new GString(tmp->htext);
      
			if(passedFirst){
				fprintf(f,",");
			}
			
			fprintf(f, "{\"top\":%d,\"left\":%d,", xoutRound(tmp->yMin+this->movey), xoutRound(tmp->xMin+this->movex));	
			fprintf(f, "\"width\":%d,\"height\":%d,", xoutRound(tmp->xMax-tmp->xMin), xoutRound(tmp->yMax-tmp->yMin));
			fprintf(f, "\"font\":%d,\"data\":\"", tmp->fontpos);
			
			if (tmp->fontpos!=-1){
				str1=fonts->getCSStyle(tmp->fontpos, str);
			}
			
			fputs(str1->getCString(),f);
			fprintf(f,"\"}");
			passedFirst = true;
				
		}
	}

	fputs("]}", f);

}

void HtmlPage::dump(FILE *f, int pageNum, GBool passedFirstPage, int totalPages) {
	dumpAsXML(f, pageNum, passedFirstPage, totalPages);
}

void HtmlPage::clear() {
	HtmlString *p1, *p2;

	if (curStr) {
		delete curStr;
		curStr = NULL;
	}

	for (p1 = yxStrings; p1; p1 = p2) {
		p2 = p1->yxNext;
		delete p1;
	}

	yxStrings = NULL;
	xyStrings = NULL;
	yxCur1 = yxCur2 = NULL;

	fontsPageMarker = fonts->size();

	delete links;
	links=new XmlLinks();
	
}

void HtmlPage::setDocName(char *fname){
	DocName=new GString(fname);
}

void HtmlPage::updateCharSpace(GfxState *state){
	charspace = state->getCharSpace();
}

//------------------------------------------------------------------------
// HtmlMetaVar
//------------------------------------------------------------------------

HtmlMetaVar::HtmlMetaVar(char *_name, char *_content) {
	name = new GString(_name);
	content = new GString(_content);
}

HtmlMetaVar::~HtmlMetaVar(){
	delete name;
	delete content;
} 

//------------------------------------------------------------------------
// ImgOutputDev
//------------------------------------------------------------------------

ImgOutputDev::ImgOutputDev(char *fileName, char *title, char *author, char *keywords, char *subject, char *date, char *extension, GBool rawOrder, GBool textAsJSON, GBool compressData, int firstPage, GBool outline, int numPages) {

	char *htmlEncoding;
	this->numPages = numPages;
	fContentsFrame = NULL;
	docTitle = new GString(title);
	pages = NULL;
	dumpJPEG=gFalse;
	//write = gTrue;
	this->rawOrder = rawOrder;
	this->textAsJSON = textAsJSON;
	this->compressData = compressData;
	this->doOutline = outline;

	ok = gFalse;
	passedFirstPage = gFalse;
	imgNum=1;
	
	needClose = gFalse;
	
	pages = new HtmlPage(rawOrder, textAsJSON, compressData, extension);

	glMetaVars = new GList();

	if (author) glMetaVars->append(new HtmlMetaVar("author", author));  
	if (keywords) glMetaVars->append(new HtmlMetaVar("keywords", keywords));  
	if (date) glMetaVars->append(new HtmlMetaVar("date", date));  
	if (subject) glMetaVars->append(new HtmlMetaVar("subject", subject));

	maxPageWidth = 0;
	maxPageHeight = 0;

	pages->setDocName(fileName);
	Docname=new GString(fileName);

	page=stdout;

	fputs("[",page);

	ok = gTrue; 

}

ImgOutputDev::~ImgOutputDev() {

	XmlFont::clear(); 

	delete Docname;
	delete docTitle;

	deleteGList(glMetaVars, HtmlMetaVar);

	fputs("]",page);
	fclose(page);

	if (pages) delete pages;

}

void ImgOutputDev::startPage(int pageNum, GfxState *state,double crop_x1, double crop_y1, double crop_x2, double crop_y2) {

	double x1,y1,x2,y2;

	state->transform(crop_x1,crop_y1,&x1,&y1);
	state->transform(crop_x2,crop_y2,&x2,&y2);

	if(x2<x1) {double x3=x1;x1=x2;x2=x3;}
	if(y2<y1) {double y3=y1;y1=y2;y2=y3;}

	pages->movex = -(int)x1;
	pages->movey = -(int)y1; 

	this->pageNum = pageNum;

	GString *str=basename(Docname);

	pages->clear(); 

	if(!noframes) {
		if (fContentsFrame) {
			if (complexMode)
				fprintf(fContentsFrame,"<A href=\"%s-%d.html\"",str->getCString(),pageNum);
			else 
				fprintf(fContentsFrame,"<A href=\"%ss.html#%d\"",str->getCString(),pageNum);
			
			fprintf(fContentsFrame," target=\"contents\" >Page %d</a><br>\n",pageNum);
		}
	}

	pages->pageWidth = (int)(x2-x1);
	pages->pageHeight = (int)(y2-y1);

	delete str;

} 

void ImgOutputDev::endPage() {

	pages->conv();
	pages->coalesce();
	pages->dump(page, pageNum, passedFirstPage,(this->numPages));
	passedFirstPage = gTrue;

	// I don't yet know what to do in the case when there are pages of different
	// sizes and we want complex output: running ghostscript many times 
	// seems very inefficient. So for now I'll just use last page's size
	maxPageWidth = pages->pageWidth;
	maxPageHeight = pages->pageHeight;

}

void ImgOutputDev::updateFont(GfxState *state) {
	pages->updateFont(state);
}

void ImgOutputDev::beginString(GfxState *state, GString *s) {
	pages->beginString(state, s);
}

void ImgOutputDev::endString(GfxState *state) {
	pages->endString();
}

void ImgOutputDev::drawChar(GfxState *state, double x, double y, double dx, double dy, double originX, double originY, CharCode code, int nBytes, Unicode *u, int uLen) {
	if ( !showHidden && (state->getRender() & 3) == 3) {
		return;
	}
	pages->addChar(state, x, y, dx, dy, originX, originY, u, uLen);
}

void ImgOutputDev::updateCharSpace(GfxState *state) {
	pages->updateCharSpace(state);
}
