/**********************************************************************************/
/* kGUI - kguipolygon2.cpp                                                        */
/*                                                                                */
/* Programmed by (See below)                                                      */
/*                                                                                */
/* http://code.google.com/p/kgui/	                                              */
/*                                                                                */
/*    kGUI is free software; you can redistribute it and/or modify                */
/*    it under the terms of the GNU Lesser General Public License as published by */
/*    the Free Software Foundation; version 2.                                    */
/*                                                                                */
/*    kGUI is distributed in the hope that it will be useful,                     */
/*    but WITHOUT ANY WARRANTY; without even the implied warranty of              */
/*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               */
/*    GNU General Public License for more details.                                */
/*                                                                                */
/*    http://www.gnu.org/licenses/lgpl.txt                                        */
/*                                                                                */
/*    You should have received a copy of the GNU General Public License           */
/*    along with kGUI; if not, write to the Free Software                         */
/*    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA  */
/*                                                                                */
/**********************************************************************************/

/* 
Concave Polygon Scan Conversion
by Paul Heckbert
from "Graphics Gems", Academic Press, 1990
*/

/*
 * concave: scan convert nvert-sided concave non-simple polygon
 * with vertices at (point[i].x, point[i].y) for i in 
 * [0..nvert-1] within the window win by
 * calling spanproc for each visible span of pixels.
 * Polygon can be clockwise or counterclockwise.
 * Algorithm does uniform point sampling at pixel centers.
 * Inside-outside test done by Jordan's rule: a point is 
 * considered inside if an emanating ray intersects the polygon 
 * an odd number of times.
 * drawproc should fill in pixels from xl to xr inclusive on scanline y,
 * e.g:
 *	drawproc(y, xl, xr)
 *	int y, xl, xr;
 *	{
 *	    int x;
 *	    for (x=xl; x<=xr; x++)
 *			pixel_write(x, y, pixelvalue);
 *	}
 *
 *  Paul Heckbert	30 June 81, 18 Dec 89
 */

#include "kgui.h"

typedef struct {		/* a polygon edge */
    double x;	/* x coordinate of edge's intersection with current scanline */
    double dx;	/* change in x with respect to y */
    int i;	/* edge number: edge i goes from pt[i] to pt[i+1] */
} Edge;

static int n;			/* number of vertices */
static kGUIDPoint2 *pt;		/* vertices */

static int nact;		/* number of active edges */
static Edge *active;	/* active edge list:edges crossing scanline y */

/* comparison routines for qsort */
static int compare_ind(const void *u, const void *v)
{
	const int *iu=(const int *)u;
	const int *iv=(const int *)v;

	if(pt[*iu].y == pt[*iv].y)
		return 0;
	return pt[*iu].y < pt[*iv].y ? -1 : 1;
}

static int compare_active(const void *u, const void *v)
{
	const Edge *eu=(const Edge *)u;
	const Edge *ev=(const Edge *)v;

	if(eu->x == ev->x)
		return(0);
	return eu->x < ev->x ? -1 : 1;
}

static void pdelete(int i)		/* remove edge i from active list */
{
    int j;

    for (j=0; j<nact && active[j].i!=i; j++);
    if (j>=nact)
		return;
		/* edge not in active list; happens at win->y0*/
    nact--;
    memmove( &active[j],&active[j+1], (nact-j)*sizeof active[0]);
}

static void pinsert(int i, double y)		/* append edge i to end of active list */
{
    int j;
    double dx;
    kGUIDPoint2 *p, *q;

    j = i<n-1 ? i+1 : 0;
    if (pt[i].y < pt[j].y)
	{
		p = &pt[i]; q = &pt[j];
	}
    else
	{
		p = &pt[j];
		q = &pt[i];
	}
    /* initialize x position at intersection of edge with scanline y */
    active[nact].dx = dx = ((q->x-p->x))/((q->y-p->y));
    active[nact].x = dx*(y-p->y)+p->x;
    active[nact].i = i;
    nact++;
}

void kGUI::DrawPoly(int nvert,kGUIDPoint2 *point,kGUIColor c,double alpha)
{
    int k, i, j;
	double y, y0,y1,xl,xr;
	int *ind;	/* list of vertex indices, sorted by pt[ind[j]].y */

	n = nvert;
    pt = point;

    assert (n>1,"not enough points in polygon!");
	
	ind=new int[n];
	active = new Edge[n];

    /* create y-sorted array of indices ind[k] into vertex list */
    for (k=0; k<n; k++)
		ind[k] = k;
	/* sort ind by pt[ind[k]].y */
	qsort(ind, n, sizeof ind[0], compare_ind);

	m_subpixcollector.SetColor(c,alpha);

    nact = 0;				/* start with empty active list */
    k = 0;				/* ind[k] is next vertex to process */
    y0 = (max((double)m_clipcorners.ty, pt[ind[0]].y));
										/* ymin of polygon */
    y1 = (min((double)m_clipcorners.by, pt[ind[n-1]].y));
										/* ymax of polygon */
	m_subpixcollector.SetColor(c,alpha);
	m_subpixcollector.SetBounds(y0,y1);

	for (y=y0; y<=y1; y+=1.0f)
	{
		/* step through scanlines */

		/* scanline y is at y+xROUND in continuous coordinates */
		/* Check vertices between previous scanline  */
		/* and current one, if any */

		for (; k<n && pt[ind[k]].y<=y; k++)
		{
	   		/* to simplify, if pt.y=y+xROUND, pretend it's above */
	   		/* invariant: y-.5 < pt[i].y <= y+xROUND */
	    	i = ind[k];	
	   	   /*
	     	* insert or delete edges before and after
			* vertex i  (i-1 to i, and i to i+1) from active 				* list if they cross scanline y
	    	*/
	    	j = i>0 ? i-1 : n-1;	/* vertex previous to i */
	    	if (pt[j].y <= y)
			/* old edge, remove from active list */
				pdelete(j);
	    	else if (pt[j].y > y)
			/* new edge, add to active list */
				pinsert(j, y);
	    	j = i<n-1 ? i+1 : 0;	/* vertex next after i */
	    	if (pt[j].y <= y)
			/* old edge, remove from active list */
				pdelete(i);
	    	else if (pt[j].y > y)
			/* new edge, add to active list */
				pinsert(i, y);
		}

		/* sort active edge list by active[j].x */
		qsort(active, nact, sizeof active[0], compare_active);

		/* draw horizontal segments for scanline y */
		for (j=0; j<nact; j+=2)
		{ /* draw horizontal segments */
		/* span 'tween j & j+1 is inside, span tween */
		/* j+1 & j+2 is outside */
	    	xl = active[j].x;	/* left end of span */
	    	if (xl<(double)m_clipcorners.lx)
				xl = (double)m_clipcorners.lx;
	    	xr = active[j+1].x;
										/* right end of span */
	    	if (xr>(double)m_clipcorners.rx)
				xr = (double)m_clipcorners.rx;
	    	if (xl<=xr)
				m_subpixcollector.AddRect(xl,y,xr-xl,1.0f);
			/* increment edge coords */
	    	active[j].x += active[j].dx;
	    	active[j+1].x += active[j+1].dx;
		  }
    }
	delete ind;
	delete active;
	m_subpixcollector.Draw();
}

void kGUI::DrawPolyLine(int nvert,kGUIDPoint2 *point,kGUIColor c)
{
	int i;
	for(i=0;i<(nvert-1);++i)
		DrawLine(point[i].x,point[i].y,point[i+1].x,point[i+1].y,c);
}

#define MAXENDPOINTS 20

static void Proj(kGUIDPoint2 *out,double x,double y,double r,double a)
{
	out->x=x-(cos(a)*r);
	out->y=y-(sin(a)*r);
}

void kGUI::DrawFatLine(double x1,double y1,double x2,double y2,kGUIColor c,double radius,double alpha)
{
	kGUIDPoint2 ends[2];

	ends[0].x=x1;
	ends[0].y=y1;
	ends[1].x=x2;
	ends[1].y=y2;
	DrawFatPolyLine(2,ends,c,radius,alpha);
}

static double Diff(double h1,double h2)
{
	double d;

	d=h1-h2;
	while(d>PI)
		d-=(PI*2);
	while(d<(-PI))
		d+=(PI*2);
	return(d);
}

static double Cross(kGUIDPoint2 *p1,kGUIDPoint2 *p2,kGUIDPoint2 *p3)
{
	return ( ((double)p2->x - p1->x)*((double)p3->y - p1->y) - ((double)p2->y - p1->y)*((double)p3->x - p1->x) );
}

/* convert to a polygon then draw using the poly code */

void kGUI::DrawFatPolyLine(unsigned int nvert,kGUIDPoint2 *point,kGUIColor c,double radius,double alpha)
{
	unsigned int i,j,numep,numinsidepoints,numcp,pass;
	unsigned int numout;
	double lastheading,heading,hdelta,lastlen,len;
	double step,estep;
	double h,dist;
	kGUIDPoint2 ip2;
	kGUIDPoint2 *p1;
	kGUIDPoint2 *p2;
	kGUIDPoint2 *op;

	if(nvert<2)
		return;

	/* last point */
	numinsidepoints=nvert-2;
	p1=point;
	p2=point+1;

	/* make the number of endpoints vary depending on thickness */
	numep=min(MAXENDPOINTS,(int)(radius+1.0f));
	/* end point step to cover 180 degrees */
	estep=PI/(numep-1);

	if(m_dfatpoints.GetNumEntries()<(MAXENDPOINTS*nvert*2))
		m_dfatpoints.Alloc(MAXENDPOINTS*nvert*2,false);

	/* number of out points */
	numout=0;
	op=m_dfatpoints.GetArrayPtr();

	heading=atan2(p2->y-p1->y,p2->x-p1->x);
	len=hypot(p2->y-p1->y,p2->x-p1->x);
	for(pass=0;pass<2;++pass)
	{
		/* build curved end for first point */
		h=(heading-(PI/2));
		for(i=0;i<numep;++i)
		{
			if(!pass)
				Proj(op,p1->x,p1->y,radius,h);
			else
				Proj(op,p2->x,p2->y,radius,h);
			++op;
			++numout;
			h+=estep;
		}

		/* ok, generate top edge */
		for(j=0;j<numinsidepoints;++j)
		{
			lastheading=heading;
			lastlen=len;
			if(pass)
			{
				--p1;
				--p2;
			}
			else
			{
				++p1;
				++p2;
			}
			if(!pass)
				heading=atan2(p2->y-p1->y,p2->x-p1->x);
			else
				heading=atan2(p1->y-p2->y,p1->x-p2->x);
			len=hypot(p1->y-p2->y,p1->x-p2->x);
			h=(lastheading+(PI/2));
			hdelta=Diff(heading,lastheading);
			numcp=min(MAXENDPOINTS,(int)abs(hdelta*radius*0.35f)+3);			/* number of points inserted  for the curve curved points */
			step=hdelta/(numcp-1);

			/* is this an inside or outside angle? */
			Proj(op,p1->x,p1->y,0.0f,0.0f);
			Proj(op+1,p1->x,p1->y,radius,h);
			Proj(op+2,p1->x,p1->y,radius,h+hdelta);
			if((Cross(op,op+1,op+2)<0.0f))
			{
				dist=-(radius*.5)*tan(hdelta*.5f);
				if(dist>len || dist>lastlen)
				{
					h=(heading+lastheading+(PI/2));
					/* put in a single point please */
					if(!pass)
						Proj(&ip2,p1->x,p1->y,radius,h);
					else
						Proj(&ip2,p2->x,p2->y,radius,h);
				}
				else
				{
					if(!pass)
						Proj(&ip2,p1->x,p1->y,(dist+radius)*2,h+hdelta*0.5f);
					else
						Proj(&ip2,p2->x,p2->y,(dist+radius)*2,h+hdelta*0.5f);
					h+=PI;
					for(i=0;i<numcp;++i)
					{
						Proj(op,ip2.x,ip2.y,radius,h);
						++op;
						++numout;
						h+=step;
					}
				}
			}
			else
			{
				for(i=0;i<numcp;++i)
				{
					if(!pass)
						Proj(op,p1->x,p1->y,radius,h);
					else
						Proj(op,p2->x,p2->y,radius,h);
					++op;
					++numout;
					h+=step;
				}
			}
		}

		heading+=PI;	/* go back 180 degrees */
	}
	assert(numout<=(MAXENDPOINTS*nvert*2),"Not enough allocated error!");
	op=m_dfatpoints.GetArrayPtr();
	DrawPoly(numout,op,c,alpha);
}

bool kGUI::DrawLine(double x1,double y1,double x2,double y2,kGUIColor c,double alpha)
{
	double dx,dy,minx,maxx,miny,maxy;

	if(x1<=x2)
	{
		minx=x1;
		maxx=x2;
	}
	else
	{
		maxx=x1;
		minx=x2;
	}
	if(y1<=y2)
	{
		miny=y1;
		maxy=y2;
	}
	else
	{
		maxy=y1;
		miny=y2;
	}
	if(OffClip((int)minx,(int)miny,(int)maxx,(int)maxy)==true)
		return(false);

	m_subpixcollector.SetBounds(y1,y2);
	m_subpixcollector.SetColor(c,alpha);

	dx=x2-x1;
	dy=y2-y1;

	if(abs(dx)>abs(dy))
	{
		double x,stepx;
		double stepy=dy/abs(dx);
		double y;
		double length=abs(dx);
		double size;

		if(dx>0.0f)
			stepx=1.0f;
		else
			stepx=-1.0f;
		y=y1;
		x=x1;
		do
		{
			size=min(length,1.0f);
			m_subpixcollector.AddRect(x,y,size,size);
			x+=stepx;
			y+=stepy;
			length-=1.0f;
		}while(length>0.0f);
	}
	else
	{
		double y,stepy;
		double stepx=(double)dx/(double)abs(dy);
		double x;
		double length=abs(dy);
		double size;

		if(dy>0)
			stepy=1.0f;
		else if(dy<0)
			stepy=-1.0f;
		else
			stepy=0.0f;
		x=x1;
		y=y1;
		do
		{
			size=min(length,1.0f);
			m_subpixcollector.AddRect(x,y,size,size);
			x+=stepx;
			y+=stepy;
			length-=1.0f;
		}while(length>0.0f);
	}
	m_subpixcollector.Draw();
	return(true);
}

/*******************************************************************/

kGUISubPixelCollector::kGUISubPixelCollector()
{
	m_lines.Init(2048,256);
	m_chunks.Init(8192,256);
}

void kGUISubPixelCollector::SetBounds(double y1,double y2)
{
	int y;
	SUBLINE_DEF *list;

	/* todo, make sure lines array is big enough */

	m_chunkindex=0;
	m_topy=(int)min(y1,y2);
	if(m_topy<kGUI::m_clipcorners.ty)
		m_topy=kGUI::m_clipcorners.ty;
	m_bottomy=(int)max(y1,y2)+1;
	if(m_bottomy>=kGUI::m_clipcorners.by)
		m_bottomy=kGUI::m_clipcorners.by-1;
	list=m_lines.GetArrayPtr();
	for(y=m_topy;y<=m_bottomy;++y)
	{
		list->chunk=0;
		list++;
	}
}

void kGUISubPixelCollector::SetColor(kGUIColor c,double alpha)
{
	int r,g,b;

	DrawColorToRGB(c,r,g,b);
	m_red=(double)r;
	m_green=(double)g;
	m_blue=(double)b;
	m_alpha=alpha;
}

void kGUISubPixelCollector::AddRect(double x,double y,double w,double h)
{
	int ty,by;
	double rx,weight;

	/* todo, in init code copy clipcorners and make doubles */
	rx=x+w;
	if(x<(double)kGUI::m_clipcorners.lx)
		x=(double)kGUI::m_clipcorners.lx;
	if(rx>(double)kGUI::m_clipcorners.rx)
		rx=(double)kGUI::m_clipcorners.rx;
	if(rx<=x)
		return;	/* off */

	/* split into integer raster line chunks */
	ty=(int)y;
	by=(int)(y+h);

	/* is this all on a single line? */
	if(ty==by)
		AddChunk(ty,x,rx,h);
	else
	{
		/* calc weight of top line */
		weight=(double)(ty+1)-y;
		AddChunk(ty,x,rx,weight);
		h-=weight;

		/* add full chunks */
		while(h>=1.0f)
		{
			AddChunk(++ty,x,rx,1.0f);
			h-=1.0f;
		}
		if(h>0.0f)
			AddChunk(++ty,x,rx,h);
	}
}

void kGUISubPixelCollector::AddChunk(int y,double lx,double rx,double weight)
{
	int lineindex;
	SUBLINEPIX_DEF *chunk;
	SUBLINE_DEF *line;
	SUBLINEPIX_DEF *prev;

	/* off of clip area? */
	if((y<m_topy) || (y>m_bottomy))
		return;

	lineindex=y-m_topy;
	line=m_lines.GetEntryPtr(lineindex);
	prev=line->chunk;

	chunk=m_chunks.GetEntryPtr(m_chunkindex++);
	chunk->next=prev;
	chunk->weight=weight;
	chunk->leftx=lx;
	chunk->width=rx-lx;

	if(rx==(double)kGUI::m_clipcorners.rx)
		rx-=1.0f;

	line->chunk=chunk;
	if(!prev)
	{
		line->leftx=lx;
		line->rightx=rx;
	}
	else
	{
		line->leftx=min(lx,line->leftx);
		line->rightx=max(rx,line->rightx);
	}
}

/* ok done, collecting, now draw */
void kGUISubPixelCollector::Draw(void)
{
	int br,bg,bb;
	int newr,newg,newb;
	SUBLINE_DEF *lines;
	SUBLINEPIX_DEF *chunk;
	int x,y,lx,rx,clx,crx;
	double m_weights[1024];
	double weight,bweight,width,fwidth;
	kGUIColor *cp;

	lines=m_lines.GetArrayPtr();
	for(y=m_topy;y<=m_bottomy;++y)
	{
		assert(y>=0 && y<kGUI::m_clipcorners.by,"Error!");
		/* process a raster line */
		chunk=lines->chunk;
		if(chunk)
		{
			lx=(int)lines->leftx;
			rx=(int)lines->rightx;
			for(x=lx;x<=rx;++x)
				m_weights[x]=0.0f;

			do
			{
				weight=chunk->weight;
				clx=(int)chunk->leftx;
				width=chunk->width;
				crx=(int)(chunk->leftx+width);
				if(clx==crx)
					m_weights[clx]+=width*weight;
				else
				{
					fwidth=1.0f-(chunk->leftx-(double)clx);
					m_weights[clx]+=fwidth*weight;
					++clx;
					width-=fwidth;
					while(width>=1.0f)
					{
						m_weights[clx]+=weight;
						width-=1.0f;
						++clx;
					}
					if(width>0.0f)
						m_weights[clx]+=width*weight;
				}
				chunk=chunk->next;
			}while(chunk);

			/* ok, now blend line */
			cp=kGUI::GetSurfacePtrC(lx,y);
			for(x=lx;x<=rx;++x)
			{
				assert(x>=0 && x<kGUI::m_clipcorners.rx,"Error!");

				weight=m_weights[x]*m_alpha;
				bweight=1.0f-weight;

				DrawColorToRGB(*(cp),br,bg,bb);
				newr=(int)((m_red*weight)+(br*bweight));
				newg=(int)((m_green*weight)+(bg*bweight));
				newb=(int)((m_blue*weight)+(bb*bweight));
				*(cp++)=DrawColor(newr,newg,newb);
			}

		}
		++lines;
	}
}

