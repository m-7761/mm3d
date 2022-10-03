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
#include "bsptree.h"
#include "glmath.h"
#include "log.h"
#include "model.h"

struct BspTree::Poly //REMOVE ME?
{
	const Model::Triangle *triangle;

	double coord[3][3];
	double drawNormals[3][3];

	float s[3],t[3]; // texture coordinates

	double d; // dot product (plane distance)

	double *BspTree::Poly::norm()
	{
		return triangle->m_flatSource;
	}
	void BspTree::Poly::calculateD()
	{
		d = dot_product(coord[0],norm());
	}

	double intersection(double *p1, double *p2, double *po);

	int _render(Draw&, int compare);
};
struct BspTree::Node : public Poly //self
{
	void addChild(Node *n);

	int render(double *point, Draw&, int compare);

	void splitNodes(int i1, int i2, int i3,
			double *p1, double *p2,Node *n1, Node *n2,
			double place1, double place2);

	void splitNode(int i1, int i2, int i3,
			double *p1, Node *n1, double place);

	Node *left, *right; //self
	
	static Node *get(); void release();
	Node():left(),right(){ s_allocated++; } //self
	~Node(){ s_allocated--; }
};

void BspTree::clear()
{
	if(m_root) m_root->release(); m_root = nullptr;
}

static bool bsptree_equiv(double rhs, double lhs)
{
	return fabs(rhs-lhs)<0.0001f;
}

double BspTree::Poly::intersection(double *p1, double *p2, double *po)
{
	double *abc = norm(), p2_p1[3];

	for(int i=3;i-->0;) p2_p1[i] = p2[i]-p1[i];

	double t = (d-dot_product(abc,p1))/dot_product(abc,p2_p1);

	lerp3(po,p1,p2,t); return t;
}

void BspTree::addTriangle(Model *m, unsigned t)
{
	Node *np = Node::get();
	auto *tp = m->getTriangleList()[t];
	auto &vl = m->getVertexList();
	for(int i=3;i-->0;)
	{
		auto *vp = vl[tp->m_vertexIndices[i]];
		memcpy(np->coord[i],vp->m_absSource,3*sizeof(double));
		memcpy(np->drawNormals[i],tp->m_normalSource[i],3*sizeof(double));
		np->s[i] = tp->m_s[i];
		np->t[i] = tp->m_t[i];
	}
	np->triangle = tp; np->calculateD();

	if(m_root) m_root->addChild(np); 
	else m_root = np;
}

void BspTree::render(double *point, Draw &context)
{
	if(!m_root) return;
	
	context.bsp->_drawMaterial(context,m_root->triangle->m_group);

	//glEnable(GL_TEXTURE_2D); //???
	{
		glBegin(GL_TRIANGLES);
		m_root->render(point,context,m_root->triangle->m_group);
		glEnd();
	}
	//glDisable(GL_TEXTURE_2D); //NEW

	//2021: restore in case m_accumulate used GL_ONE
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}
int BspTree::Node::render(double *point, Draw &context, int compare)
{
	if(dot_product(norm(),point)<d)
	{
		if(right)
		compare = right->render(point,context,compare);
		compare = _render(context,compare);
		if(left)
		compare = left->render(point,context,compare);
	}
	else
	{
		if(left)
		compare = left->render(point,context,compare);
		compare = _render(context,compare);
		if(right)
		compare = right->render(point,context,compare);
	}

	return compare; //2021
}
int BspTree::Poly::_render(Draw &context, int compare)
{
	if(compare!=triangle->m_group)
	{
		compare = triangle->m_group; //material

		glEnd();

		context.bsp->_drawMaterial(context,compare);

		glBegin(GL_TRIANGLES);
	}

	if(triangle->visible(context.layers))	
	for(int i=0;i<3;i++)
	{
		glTexCoord2f(s[i],t[i]);
		glNormal3dv(drawNormals[i]);
		glVertex3dv(coord[i]);
	}	

	return compare; //2021
}

void BspTree::Node::splitNodes(int i1, int i2, int i3,
		double *p1, double *p2,
		BspTree::Node *n1, BspTree::Node *n2,
		double place1, double place2)
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
	drawNormals[i1],drawNormals[i2],place1);
	n1->s[0] = lerp(s[i1],s[i2],place1);
	n1->t[0] = lerp(t[i1],t[i2],place1);
	n1->s[1] = s[i2];
	n1->t[1] = t[i2];
	n1->s[2] = s[i3];
	n1->t[2] = t[i3];

	//n1->calculateNormal();
	n1->triangle = triangle;
	n1->d = d; //2022

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
	drawNormals[i1],drawNormals[i2],place1);	
	n2->s[0] = lerp(s[i1],s[i2],place1);
	n2->t[0] = lerp(t[i1],t[i2],place1);
	n2->s[1] = s[i3];
	n2->t[1] = t[i3];
	lerp3(n2->drawNormals[2], //2022
	drawNormals[i1],drawNormals[i3],place2);
	n2->s[2] = lerp(s[i1],s[i3],place2);
	n2->t[2] = lerp(t[i1],t[i3],place2);

	//n2->calculateNormal();
	n2->triangle = triangle;
	n2->d = d; //2022

	for(int i=3;i-->0;)
	{
		coord[i2][i] = p1[i];
		coord[i3][i] = p2[i];
		//drawNormals[i2][i] = norm[i]; //???
		//drawNormals[i3][i] = norm[i]; //???
	}
	lerp3(drawNormals[i2], //2022
	drawNormals[i1],drawNormals[i2],place1);
	s[i2] = lerp(s[i1],s[i2],place1);	
	t[i2] = lerp(t[i1],t[i2],place1);
	lerp3(drawNormals[i3], //2022
	drawNormals[i1],drawNormals[i3],place2);
	s[i3] = lerp(s[i1],s[i3],place2);
	t[i3] = lerp(t[i1],t[i3],place2);

	//calculateD(); //???
}

void BspTree::Node::splitNode(int i1, int i2, int i3,
		double *p1, BspTree::Node *n1, double place)
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

	//n1->calculateNormal();
	n1->triangle = triangle;
	n1->d = d; //2022

	for(int i=3;i-->0;)
	{
		coord[i2][i] = p1[i];

		//drawNormals[i2][i] = norm[i]; //???
	}
	lerp3(drawNormals[i2], //2022
	drawNormals[i2],drawNormals[i3],place);
	s[i2] = lerp(s[i2],s[i3],place);
	t[i2] = lerp(t[i2],t[i3],place);

	//calculateD(); //???
}

void BspTree::Node::addChild(Node *n)
{
	double d1 = dot_product(norm(),n->coord[0]);
	double d2 = dot_product(norm(),n->coord[1]);
	double d3 = dot_product(norm(),n->coord[2]);

	int i1,i2,i3;
	if(i1=!bsptree_equiv(d1,d)) i1 = d1<d?-1:1;
	if(i2=!bsptree_equiv(d2,d)) i2 = d2<d?-1:1;
	if(i3=!bsptree_equiv(d3,d)) i3 = d3<d?-1:1;

	// This will catch co-plane also... which should be fine
	if(i1<=0&&i2<=0&&i3<=0)
	{
		if(left) left->addChild(n);
		else left = n;
		return;
	}
	else if(i1>=0&&i2>=0&&i3>=0)
	{
		if(right) right->addChild(n);
		else right = n;
		return;
	}

	double place1,p1[3];
	double place2,p2[3];

	if(i1==0||i2==0||i3==0)
	{
		// one of the vertices is on the plane
		
		Node *n1 = Node::get();
		if(i1==0)
		{
			place1 = intersection(n->coord[1],n->coord[2],p1);

			n->splitNode(0,1,2,p1,n1,place1);

			if(i2<0)
			{
				if(right) addChild(n);
				else right = n;

				if(left) left->addChild(n1);
				else left = n1;
			}
			else
			{
				if(right) addChild(n1);
				else right = n1;

				if(left) left->addChild(n);
				else left = n;
			}
		}
		else if(i2==0)
		{
			place1 = intersection(n->coord[2],n->coord[0],p1);

			n->splitNode(1,2,0,p1,n1,place1);

			if(i1<0)
			{
				if(right) addChild(n1);
				else right = n1;

				if(left) left->addChild(n);
				else left = n;
			}
			else
			{
				if(right) addChild(n);
				else right = n;

				if(left) left->addChild(n1);
				else left = n1;
			}
		}
		else if(i3==0)
		{
			place1 = intersection(n->coord[0],n->coord[1],p1);

			n->splitNode(2,0,1,p1,n1,place1);

			if(i1<0)
			{
				if(right) addChild(n);
				else right = n;

				if(left) left->addChild(n1);
				else left = n1;
			}
			else
			{
				if(right) addChild(n1);
				else right = n1;

				if(left) left->addChild(n);
				else left = n;
			}
		}
		else assert(0); //MEMORY LEAK?
	}
	else
	{
		Node *n1 = Node::get();
		Node *n2 = Node::get();
		if(i1==i2)
		{
			place1 = intersection(n->coord[2],n->coord[0],p1);
			place2 = intersection(n->coord[2],n->coord[1],p2);
		
			n->splitNodes(2,0,1,p1,p2,n1,n2,place1,place2);

			if(i3<0)
			{
				n1->left = n2;

				if(right) right->addChild(n1);
				else right = n1;

				if(left) left->addChild(n);
				else left = n;
			}
			else
			{
				n1->right = n2;

				if(left) left->addChild(n1);
				else left = n1;

				if(right) right->addChild(n);
				else right = n;
			}
		}
		else if(i1==i3)
		{
			place1 = intersection(n->coord[1],n->coord[2],p1);
			place2 = intersection(n->coord[1],n->coord[0],p2);

			n->splitNodes(1,2,0,p1,p2,n1,n2,place1,place2);

			if(i2<0)
			{
				n1->left = n2;

				if(right) right->addChild(n1);
				else right = n1;

				if(left) left->addChild(n);
				else left = n;
			}
			else
			{
				n1->right = n2;

				if(left) left->addChild(n1);
				else left = n1;

				if(right) right->addChild(n);
				else right = n;
			}
		}
		else if(i2==i3)
		{
			place1 = intersection(n->coord[0],n->coord[1],p1);
			place2 = intersection(n->coord[0],n->coord[2],p2);

			n->splitNodes(0,1,2,p1,p2,n1,n2,place1,place2);

			if(i1<0)
			{
				n1->left = n2;

				if(right) right->addChild(n1);
				else right = n1;

				if(left) left->addChild(n);
				else left = n;
			}
			else
			{
				n1->right = n2;

				if(left) left->addChild(n1);
				else left = n1;

				if(right) right->addChild(n);
				else right = n;
			}
		}
		else assert(0); //MEMORY LEAK?
	}
}

std::vector<BspTree::Node*> BspTree::s_recycle;
int BspTree::s_allocated = 0;
BspTree::Node *BspTree::Node::get()
{
	if(!s_recycle.empty())
	{
		Node *n = s_recycle.back();
		s_recycle.pop_back();
	//	n->self = nullptr;
		n->left = nullptr;
		n->right = nullptr;
		return n;
	}
	else return new Node;
}
void BspTree::Node::release()
{
	if(left) left->release();
	
	//if(self) release();
	s_recycle.push_back(this);
	
	if(right) right->release();

	//s_recycle.push_back(this);
}

int BspTree::flush()
{
	for(auto*ea:s_recycle) delete ea;

	auto ret = s_recycle.size(); s_recycle.clear(); return ret;
}

void BspTree::stats()
{
	log_debug("BspTree::Node: %d/%d\n",s_recycle.size(),s_allocated);
}

