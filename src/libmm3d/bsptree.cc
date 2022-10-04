/*  MM3D Misfit/Maverick Model 3D
 *
 * Copyright (c)2004-2007 Kevin Worcester
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place-Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * See the COPYING file for full license text.
 */

#include "mm3dtypes.h" //PCH

#include "glheaders.h"
#include "glmath.h"
//#include "bsptree.h"
#include "glmath.h"
#include "log.h"
#include "model.h"

struct Model::BspTree::Poly
{
	Poly *next;

	static Poly *get();
	Poly():next(){ s_allocated2++; }
	~Poly(){ s_allocated2--; }

	const Model::Triangle *triangle;

	double coord[3][3];
	double drawNormals[3][3];

	float s[3],t[3]; // texture coordinates

	void split(int i1, int i2, int i3, double p1[3], Poly*, double t);
	void split2(int,int,int, double[3],double p2[3], Poly*,Poly*, double,double t2);

	void _render(Draw&);
};
struct Model::BspTree::Node
{
	Node *left,*right;

	Poly *first;
	
	double d; double *abc() // plane equation
	{
		return first->triangle->m_flatSource;
	}

	void partition();
	
	enum{ LEFT=-1,SAME,RIGHT,BOTH };

	struct side_t{ int i1:4,i2:4,i3:4,result:4; };

	side_t side(Poly*);
	Poly *split(Poly*&,side_t);

	double intersection(const double p1[3], const double p2[3], double po[3]);

	void render(double point[3], Draw&);
	
	static Node *get(); void release();
	Node():left(),right(),first(){ s_allocated++; }
	~Node(){ s_allocated--; }
};

void Model::BspTree::clear()
{
	if(m_root) m_root->release(); m_root = nullptr;
}
void Model::BspTree::addTriangles(Model *m, int_list &l)
{
	if(l.empty()) return;

	if(!m_root) m_root = Node::get();
	else assert(!m_root->left&&!m_root->right);

	Poly *p = m_root->first;
	auto &tl = m->getTriangleList();
	auto &vl = m->getVertexList();
	for(int i:l)
	{
		Poly *np = Poly::get();
		auto *tp = tl[i];	
		for(int j=3;j-->0;)
		{
			auto *vp = vl[tp->m_vertexIndices[j]];
			memcpy(np->coord[j],vp->m_absSource,3*sizeof(double));
			memcpy(np->drawNormals[j],tp->m_normalSource[j],3*sizeof(double));
			np->s[j] = tp->m_s[j];
			np->t[j] = tp->m_t[j];
		}	
		np->triangle = tp;

		np->next = p; p = np;
	}
	m_root->first = p;
}
void Model::BspTree::partition()
{
	if(m_root) m_root->partition();
}
void Model::BspTree::Node::partition()
{
	d = dot_product(first->coord[0],abc());

	Poly *_l[3] = {}, **l = _l-LEFT; //YUCK!

	//NOTE: Sometimes code will try to pick a polygon
	//to divide the half-spaces more evenly. A random
	//pick might help but is messier with linked list.
	for(Poly*q,*p=first->next;p;p=q)
	{
		q = p->next;

		side_t e = side(p);

		int i = e.result; 
		
		if(i==BOTH)
		{
			p->next = nullptr; //2 on left?

			if(Poly*pp=split(p,e))
			{
				if(p->next) //2 on left?
				{
					p->next->next = l[LEFT]; l[LEFT] = p->next;
				}
				else if(pp->next) //2 on right?
				{
					pp->next->next = l[RIGHT]; l[RIGHT] = pp->next;
				}

				pp->next = l[RIGHT]; l[RIGHT] = pp;
			}
			else assert(0);

			i = LEFT;
		}
		
		p->next = l[i]; l[i] = p;
	}

	first->next = l[SAME];

	for(int i=LEFT;i<=RIGHT;i+=RIGHT-LEFT)
	{
		if(!l[i]) continue;
		
		auto np = Node::get();

		(i==LEFT?left:right) = np;

		np->first = l[i]; np->partition();
	}
}

double Model::BspTree::Node::intersection
(const double p1[3], const double p2[3], double po[3])
{
	double p2_p1[3]; for(int i=3;i-->0;) p2_p1[i] = p2[i]-p1[i];
	double t = (d-dot_product(abc(),p1))/dot_product(abc(),p2_p1);
	lerp3(po,p1,p2,t); return t;
}
void Model::BspTree::Poly::split
(int i1, int i2, int i3, double p1[3], Poly *n1, double place)
{
	for(int i=3;i-->0;)
	{
		n1->coord[0][i] = coord[i1][i];
		n1->coord[1][i] = coord[i2][i];
		n1->coord[2][i] = p1[i];

		n1->drawNormals[0][i] = drawNormals[i1][i];
		n1->drawNormals[1][i] = drawNormals[i2][i];
		//n1->drawNormals[2][i] = norm[i]; //???
	}
	n1->s[0] = s[i1];
	n1->t[0] = t[i1];
	n1->s[1] = s[i2];
	n1->t[1] = t[i2];
	lerp3(n1->drawNormals[2], //2022
	drawNormals[i2],drawNormals[i3],place);
	n1->s[2] = lerp(s[i2],s[i3],place);
	n1->t[2] = lerp(t[i2],t[i3],place);

	n1->triangle = triangle;

	for(int i=3;i-->0;)
	{
		coord[i2][i] = p1[i];

		//drawNormals[i2][i] = norm[i]; //???
	}
	lerp3(drawNormals[i2], //2022
	drawNormals[i2],drawNormals[i3],place);
	s[i2] = lerp(s[i2],s[i3],place);
	t[i2] = lerp(t[i2],t[i3],place);
}
void Model::BspTree::Poly::split2(int i1, int i2, int i3,
double p1[3], double p2[3], Poly *n1, Poly *n2, double t1, double t2)
{
	for(int i=3;i-->0;)
	{
		n1->coord[0][i] = p1[i];
		n1->coord[1][i] = coord[i2][i];
		n1->coord[2][i] = coord[i3][i];

		n1->drawNormals[1][i] = drawNormals[i2][i];
		n1->drawNormals[2][i] = drawNormals[i3][i];
		//n1->drawNormals[0][i] = norm[i]; //???
	}
	lerp3(n1->drawNormals[0], //2022
	drawNormals[i1],drawNormals[i2],t1);
	n1->s[0] = lerp(s[i1],s[i2],t1);
	n1->t[0] = lerp(t[i1],t[i2],t1);
	n1->s[1] = s[i2];
	n1->t[1] = t[i2];
	n1->s[2] = s[i3];
	n1->t[2] = t[i3];

	n1->triangle = triangle;

	for(int i=3;i-->0;)
	{
		n2->coord[0][i] = p1[i];
		n2->coord[1][i] = coord[i3][i];
		n2->coord[2][i] = p2[i];

		n2->drawNormals[1][i] = drawNormals[i3][i];
		//n2->drawNormals[0][i] = norm[i]; //???		
		//n2->drawNormals[2][i] = norm[i]; //???
	}
	lerp3(n2->drawNormals[0], //2022
	drawNormals[i1],drawNormals[i2],t1);	
	n2->s[0] = lerp(s[i1],s[i2],t1);
	n2->t[0] = lerp(t[i1],t[i2],t1);
	n2->s[1] = s[i3];
	n2->t[1] = t[i3];
	lerp3(n2->drawNormals[2], //2022
	drawNormals[i1],drawNormals[i3],t2);
	n2->s[2] = lerp(s[i1],s[i3],t2);
	n2->t[2] = lerp(t[i1],t[i3],t2);

	n2->triangle = triangle;

	for(int i=3;i-->0;)
	{
		coord[i2][i] = p1[i];
		coord[i3][i] = p2[i];
		//drawNormals[i2][i] = norm[i]; //???
		//drawNormals[i3][i] = norm[i]; //???
	}
	lerp3(drawNormals[i2], //2022
	drawNormals[i1],drawNormals[i2],t1);
	s[i2] = lerp(s[i1],s[i2],t1);	
	t[i2] = lerp(t[i1],t[i2],t1);
	lerp3(drawNormals[i3], //2022
	drawNormals[i1],drawNormals[i3],t2);
	s[i3] = lerp(s[i1],s[i3],t2);
	t[i3] = lerp(t[i1],t[i3],t2);
}
Model::BspTree::Node::side_t
Model::BspTree::Node::side(Poly *p)
{
	auto eq = [](double rhs, double lhs)
	{
		return fabs(rhs-lhs)<0.0001f;
	};
	double *n = abc();
	double d1 = dot_product(n,p->coord[0]);
	double d2 = dot_product(n,p->coord[1]);
	double d3 = dot_product(n,p->coord[2]);

	int e1,e2,e3,er;
	if(e1=!eq(d1,d)) e1 = d1<d?LEFT:RIGHT;
	if(e2=!eq(d2,d)) e2 = d2<d?LEFT:RIGHT;
	if(e3=!eq(d3,d)) e3 = d3<d?LEFT:RIGHT;

	if(e1<=0&&e2<=0&&e3<=0)
	{
		er = e1|e2|e3?LEFT:SAME;
	}
	else er = e1>=0&&e2>=0&&e3>=0?RIGHT:BOTH;

	return side_t{e1,e2,e3,er};
}
Model::BspTree::Poly *Model::BspTree::Node::split(Poly* &p, side_t e)
{
	Poly *r1,*r2;
	int i0,i1,i2; bool rtl;
	double t1,p1[3],t2,p2[3]; assert(!p->next);
	if(!e.i1||!e.i2||!e.i3)
	{
		// one of the vertices is on the plane
		
		if(!e.i1)
		{
			i0 = 0; i1 = 1; i2 = 2; rtl = e.i2<0;
		}
		else if(!e.i2)
		{
			i0 = 1; i1 = 2; i2 = 0; rtl = e.i1>=0;
		}
		else if(!e.i3)
		{
			i0 = 2; i1 = 0; i2 = 1; rtl = e.i1<0;
		}
		else //MEMORY LEAK?
		{
			assert(0); return nullptr; //2022
		}
		
		t1 = intersection(p->coord[i1],p->coord[i2],p1);

		r1 = Poly::get();

		p->split(i0,i1,i2,p1,r1,t1);
	}
	else //triangle and quadrangle (or 3 triangles IOW)
	{
		if(e.i1==e.i2)
		{
			i0 = 2; i1 = 0; i2 = 1; rtl = e.i3>=0;
		}
		else if(e.i1==e.i3)
		{
			i0 = 1; i1 = 2; i2 = 0; rtl = e.i2>=0;
		}
		else if(e.i2==e.i3)
		{
			i0 = 0; i1 = 1; i2 = 2; rtl = e.i1>=0;
		}
		else //MEMORY LEAK?
		{
			assert(0); return nullptr; //2022
		}
		
		t1 = intersection(p->coord[i0],p->coord[i1],p1);
		t2 = intersection(p->coord[i0],p->coord[i2],p2);

		r1 = Poly::get(); r2 = Poly::get();
		
		p->split2(i0,i1,i2,p1,p2,r1,r2,t1,t2);

		r1->next = r2;
	}
	if(rtl) std::swap(p,r1); return r1;
}

void Model::BspTree::render(double point[3], Draw &context)
{
	if(!m_root) return;

	Poly *p = m_root->first; assert(p);

	context.bsp_group = p->triangle->m_group;
	context.bsp_selected = false;
	context.bsp->_drawMaterial(context,context.bsp_group);

	//glEnable(GL_TEXTURE_2D); //???
	{
		glBegin(GL_TRIANGLES);
		m_root->render(point,context);
		glEnd();
	}
	//glDisable(GL_TEXTURE_2D); //NEW

	//2021: restore in case m_accumulate used GL_ONE
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	if(context.bsp_selected) glDisable(GL_LIGHT1);
}
void Model::BspTree::Node::render(double point[3], Draw &context)
{
	if(dot_product(abc(),point)<d)
	{
		if(right) right->render(point,context);

		for(Poly*p=first;p;p=p->next) p->_render(context);

		if(left) left->render(point,context);
	}
	else
	{
		if(left) left->render(point,context);

		for(Poly*p=first;p;p=p->next) p->_render(context);

		if(right) right->render(point,context);
	}
}
void Model::BspTree::Poly::_render(Draw &context)
{
	if(!triangle->visible(context.layers)) return;

	if(context.bsp_group!=triangle->m_group) 
	{
		context.bsp_group = triangle->m_group; //material

		glEnd();

		context.bsp->_drawMaterial(context,context.bsp_group);

		if(context.bsp_selected!=triangle->m_selected)
		goto selected;

		glBegin(GL_TRIANGLES);
	}
	if(context.bsp_selected!=triangle->m_selected)
	{
		context.bsp_selected = triangle->m_selected; //red light

		glEnd(); selected: //OPTIMIZING?
		glDisable(context.bsp_selected?GL_LIGHT0:GL_LIGHT1);
		glEnable(context.bsp_selected?GL_LIGHT1:GL_LIGHT0);
		glBegin(GL_TRIANGLES);
	}

	for(int i=0;i<3;i++)
	{
		glTexCoord2f(s[i],t[i]);
		glNormal3dv(drawNormals[i]);
		glVertex3dv(coord[i]);
	}
}

std::vector<Model::BspTree::Node*> Model::BspTree::s_recycle;
std::vector<Model::BspTree::Poly*> Model::BspTree::s_recycle2;
int Model::BspTree::s_allocated = 0;
int Model::BspTree::s_allocated2 = 0;
Model::BspTree::Node *Model::BspTree::Node::get()
{
	if(!s_recycle.empty())
	{
		Node *n = s_recycle.back();
		s_recycle.pop_back();
		n->first = nullptr;
		n->left = nullptr;
		n->right = nullptr;
		return n;
	}
	else return new Node;
}
void Model::BspTree::Node::release()
{
	if(left) left->release();	
	if(right) right->release();
	for(auto*p=first;p;p=p->next)
	{
		//p->release();
		s_recycle2.push_back(p);
	}
	s_recycle.push_back(this);
}
Model::BspTree::Poly *Model::BspTree::Poly::get()
{
	if(!s_recycle2.empty())
	{
		Poly *n = s_recycle2.back();
		s_recycle2.pop_back();
		n->next = nullptr;
		return n;
	}
	else return new Poly;
}

void Model::BspTree::stats()
{
	log_debug("Model::BspTree::Node: %d/%d\n",s_recycle.size(),s_allocated);
	log_debug("Model::BspTree::Poly: %d/%d\n",s_recycle2.size(),s_allocated2);
}
int Model::BspTree::flush()
{
	for(auto*ea:s_recycle) delete ea;

	auto ret = s_recycle.size(); s_recycle.clear(); return ret;
}
int Model::BspTree::flush2()
{
	for(auto*ea:s_recycle2) delete ea;

	auto ret = s_recycle2.size(); s_recycle2.clear(); return ret;
}

