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
//#include "bsptree.h"
#include "log.h"
#include "model.h"

struct Model::BspTree::Poly
{
	Poly *next;

	static Poly *get();
	Poly():next(){ s_allocated2++; }
	~Poly(){ s_allocated2--; }

	const Model::Triangle *triangle;

	struct vertex
	{
		float coord[3];
		float drawNormals[3];
		float st[2];

		void lerp(vertex &a, vertex &b, float t)
		{
			for(int i=8;i-->0;)
			coord[i] = a.coord[i]+(b.coord[i]-a.coord[i])*t;
		}

	}v[3];

	void split(int i1, int i2, int i3, Poly*, float t);
	void split2(int,int,int, Poly*,Poly*, float,float t2);
};
struct Model::BspTree::Node
{
	Node *left,*right;

	Poly *first;
	
	float d; // plane equation
	float abc_dot_product(const float p[3])
	{
		double *n = first->triangle->m_flatSource;
		return (float)(p[0]*n[0]+p[1]*n[1]+p[2]*n[2]);
	}

	void partition();
	
	enum{ LEFT=-1,SAME,RIGHT,BOTH };

	struct side_t{ int i0:4,i1:4,i2:4,result:4; };

	side_t side(Poly*);
	Poly *split(Poly*&,side_t);

	float intersection(const float p1[3], const float p2[3]);

	void render(float point[3], Draw&);
	
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
			auto &vj = np->v[j];
			auto *vp = vl[tp->m_vertexIndices[j]];
			for(int k=3;k-->0;)
			{
				vj.coord[k] = (float)vp->m_absSource[k];
				vj.drawNormals[k] = (float)tp->m_normalSource[j][k];
			}
			vj.st[0] = tp->m_s[j];
			vj.st[1] = tp->m_t[j];
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
	d = abc_dot_product(first->v[0].coord);

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

float Model::BspTree::Node::intersection(const float p1[3], const float p2[3])
{
	float p2_p1[3]; for(int i=3;i-->0;) p2_p1[i] = p2[i]-p1[i];

	return (d-abc_dot_product(p1))/abc_dot_product(p2_p1);
}
void Model::BspTree::Poly::split(int i0, int i1, int i2, Poly *n1, float t1)
{
	n1->v[2].lerp(v[i1],v[i2],t1);

	n1->v[0] = v[i0];
	n1->v[1] = v[i1];

	n1->triangle = triangle;

	v[i1] = n1->v[2];
}
void Model::BspTree::Poly::split2
(int i0, int i1, int i2, Poly *n1, Poly *n2, float t1, float t2)
{
	n1->v[0].lerp(v[i0],v[i1],t1);
	n2->v[2].lerp(v[i0],v[i2],t2);

	n1->v[1] = v[i1]; 
	n1->v[2] = v[i2];

	n1->triangle = triangle;

	n2->v[0] = n1->v[0]; 
	n2->v[1] = v[i2];

	n2->triangle = triangle;

	v[i1] = n1->v[0]; 
	v[i2] = n2->v[2];
}
Model::BspTree::Node::side_t
Model::BspTree::Node::side(Poly *p)
{
	auto eq = [](float rhs, float lhs)
	{
		return fabsf(rhs-lhs)<0.0001f;
	};
	float d0 = abc_dot_product(p->v[0].coord);
	float d1 = abc_dot_product(p->v[1].coord);
	float d2 = abc_dot_product(p->v[2].coord);

	int e0,e1,e2,er;
	if(e0=!eq(d0,d)) e0 = d0<d?LEFT:RIGHT;
	if(e1=!eq(d1,d)) e1 = d1<d?LEFT:RIGHT;
	if(e2=!eq(d2,d)) e2 = d2<d?LEFT:RIGHT;

	if(e0<=0&&e1<=0&&e2<=0)
	{
		er = e0|e1|e2?LEFT:SAME;
	}
	else er = e0>=0&&e1>=0&&e2>=0?RIGHT:BOTH;

	return side_t{e0,e1,e2,er};
}
Model::BspTree::Poly *Model::BspTree::Node::split(Poly* &p, side_t e)
{
	auto r = Poly::get(); assert(!p->next);
	bool rtl;
	if(!e.i0||!e.i1||!e.i2)
	{
		// one of the vertices is on the plane
		
		int i0,i1,i2; if(!e.i0)
		{
			i0 = 0; i1 = 1; i2 = 2; rtl = e.i1<0;
		}
		else if(!e.i1)
		{
			i0 = 1; i1 = 2; i2 = 0; rtl = e.i0>=0;
		}
		else if(!e.i2)
		{
			i0 = 2; i1 = 0; i2 = 1; rtl = e.i0<0;
		}
		else //MEMORY LEAK?
		{
			assert(0); return nullptr; //2022
		}

		p->split(i0,i1,i2,r,intersection(p->v[i1].coord,p->v[i2].coord));
	}
	else //triangle and quadrangle (or 3 triangles IOW)
	{
		int i0,i1,i2; if(e.i0==e.i1)
		{
			i0 = 2; i1 = 0; i2 = 1; rtl = e.i2>=0;
		}
		else if(e.i0==e.i2)
		{
			i0 = 1; i1 = 2; i2 = 0; rtl = e.i1>=0;
		}
		else if(e.i1==e.i2)
		{
			i0 = 0; i1 = 1; i2 = 2; rtl = e.i0>=0;
		}
		else //MEMORY LEAK?
		{
			assert(0); return nullptr; //2022
		}

		p->split2(i0,i1,i2,r,r->next=Poly::get(),
		intersection(p->v[i0].coord,p->v[i1].coord),intersection(p->v[i0].coord,p->v[i2].coord));
	}
	if(rtl) std::swap(p,r); return r;
}

void Model::BspTree::render(double point[3], Draw &context)
{
	if(!m_root) return;

	float pointf[3]; 
	for(int i=3;i-->0;) pointf[i] = (float)point[i];

	Poly *p = m_root->first; assert(p);

	context.bsp_group = p->triangle->m_group;
	context.bsp_selected = false;
	context.bsp->_drawMaterial(context,context.bsp_group);

	//glEnable(GL_TEXTURE_2D); //???
	{
		glBegin(GL_TRIANGLES);
		m_root->render(pointf,context); //point
		glEnd();
	}
	//glDisable(GL_TEXTURE_2D); //NEW

	//2021: restore in case m_accumulate used GL_ONE
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	if(context.bsp_selected) glDisable(GL_LIGHT1);
}
void Model::BspTree::Node::render(float point[3], Draw &context)
{
	bool r = abc_dot_product(point)<d;

	if(auto*n=r?right:left) n->render(point,context);

	for(Poly*p=first;p;p=p->next)
	{
		auto *t = p->triangle;

		if(!t->visible(context.layers)) continue;

		if(context.bsp_group!=t->m_group) 
		{
			context.bsp_group = t->m_group; //material

			glEnd();

			context.bsp->_drawMaterial(context,context.bsp_group);

			if(context.bsp_selected!=t->m_selected)
			goto selected;

			glBegin(GL_TRIANGLES);
		}
		if(context.bsp_selected!=t->m_selected)
		{
			context.bsp_selected = t->m_selected; //red light

			glEnd(); selected: //OPTIMIZING?
			glDisable(context.bsp_selected?GL_LIGHT0:GL_LIGHT1);
			glEnable(context.bsp_selected?GL_LIGHT1:GL_LIGHT0);
			glBegin(GL_TRIANGLES);
		}

		for(auto&v:p->v)
		{
			glTexCoord2fv(v.st);
			glNormal3fv(v.drawNormals);
			glVertex3fv(v.coord);
		}
	}

	if(auto*n=r?left:right) n->render(point,context);
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

