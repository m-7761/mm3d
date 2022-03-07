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

#include "model.h"
#include "log.h"

#ifdef MM3D_EDIT

#include "modelundo.h"

void Model::setSelectionMode(Model::SelectionModeE m)
{
	if(m!=m_selectionMode)
	{
		if(m_undoEnabled)
		{
			auto undo = new MU_SelectionMode;
			undo->setSelectionMode(m,m_selectionMode);
			sendUndo(undo);
		}

		m_selectionMode = m;
	}
}

bool Model::selectVertex(unsigned v)
{
	if(v<m_vertices.size()
	&&!m_vertices[v]->m_selected) //2019
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionVertices; //2019
		
			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectVertices);
				undo->setSelectionDifference(v,true,false);
				sendUndo(undo);
			}
		}

		m_vertices[v]->m_selected = true;

		return true;
	}
	return false;
}
bool Model::selectTriangle(unsigned t)
{
	if(t<m_triangles.size()
	&&!m_triangles[t]->m_selected) //2019
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionFaces; //2019
			
			bool o = setUndoEnabled(false);
			//selectVerticesFromTriangles();
			selectVertex(m_triangles[t]->m_vertexIndices[0]);
			selectVertex(m_triangles[t]->m_vertexIndices[1]);
			selectVertex(m_triangles[t]->m_vertexIndices[2]);
			setUndoEnabled(o);

			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectTriangles);
				undo->setSelectionDifference(t,true,false);
				sendUndo(undo);
			}
		}
		else for(int i:m_triangles[t]->m_vertexIndices)
		{
			m_vertices[i]->m_selected = true;
		}

		m_triangles[t]->m_selected = true;

		return true;
	}
	return false;
}
bool Model::selectGroup(unsigned m)
{
	if(m>=0&&m<m_groups.size())
	{
		//2022: This may have been a mistake...
		//I was adding this indescriminately...
		//I don't trust Group::m_selected
		//if(m_groups[m]->m_selected) return false; //2019

		Group *grp = m_groups[m];

		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;			
			m_changeBits |= SelectionGroups; //2019
		
			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectGroups);
				undo->setSelectionDifference(m,true,false);
				sendUndo(undo);
			}
		}

		grp->m_selected = true;

		bool o = setUndoEnabled(false); //JUST DISABLES assert CALLS

		//selectTrianglesFromGroups(); //2022: WHAT?
		{
			//selectTrianglesFromGroups calls this? is that right???
			//unselectAllTriangles();

			bool sel = false;

			for(auto i:grp->m_triangleIndices)
			{				
				auto *tp = m_triangles[i];

				if(tp->m_visible&&!tp->m_selected)
				{
					sel = true; tp->m_selected = true;
				}
			}
			if(sel&&!m_selecting) m_changeBits|=SelectionFaces; //2022

			if(sel) selectVerticesFromTriangles();
		}

		setUndoEnabled(o);

		return true;
	}
	return false;
}
bool Model::selectBoneJoint(unsigned j)
{
	if(j>=0&&j<m_joints.size()
	&&!m_joints[j]->m_selected) //2019
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionJoints; //2019

			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectJoints);
				undo->setSelectionDifference(j,true,false);
				sendUndo(undo);
			}
		}

		m_joints[j]->m_selected = true;

		return true;
	}
	return false;
}
bool Model::selectPoint(unsigned p)
{
	if(p>=0&&p<m_points.size()
	&&!m_points[p]->m_selected) //2019
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionPoints; //2019
		
			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectPoints);
				undo->setSelectionDifference(p,true,false);
				sendUndo(undo);
			}
		}

		m_points[p]->m_selected = true;

		return true;
	}
	return false;
}
bool Model::selectProjection(unsigned p)
{
	if(p>=0&&p<m_projections.size()
	&&!m_projections[p]->m_selected) //2019
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionProjections; //2019
		
			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectProjections);
				undo->setSelectionDifference(p,true,false);
				sendUndo(undo);
			}

		}

		m_projections[p]->m_selected = true;
		
		return true;
	}
	return false;
}
bool Model::selectPosition(Position p, bool how)
{
	switch(p.type)
	{
	case PT_Vertex:
		return how?selectVertex(p):unselectVertex(p); 
	case PT_Joint:
		return how?selectBoneJoint(p):unselectBoneJoint(p);		
	case PT_Point:
		return how?selectPoint(p):unselectPoint(p); 
	case PT_Projection:
		return how?selectProjection(p):unselectProjection(p); 
	}
	return false;
}

//REMOVE US
bool Model::unselectVertex(unsigned v)
{
	if(v<m_vertices.size()
	&&m_vertices[v]->m_selected) //2019
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionVertices; //2019

			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectVertices);
				undo->setSelectionDifference(v,false,true);
				sendUndo(undo);
			}
		}

		m_vertices[v]->m_selected = false; 
		
		return true;
	}
	return false;
}
bool Model::unselectTriangle(unsigned t, bool remove_me)
{
	if(t<m_triangles.size()
	&&m_triangles[t]->m_selected) //2019
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionFaces; //2019

			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectTriangles);
				undo->setSelectionDifference(t,false,true);
				sendUndo(undo);
			}
		}

		m_triangles[t]->m_selected = false;
		
		if(remove_me) //2019: MU_Select called in a loop!
		{
			//bool o = setUndoEnabled(false);			
			//selectVerticesFromTriangles(); //INSANE?!?
			{
				//BETTER: leverage new connectivty data?
				for(auto i:m_triangles[t]->m_vertexIndices)
				{
					bool selected = false;
					for(auto&ea:m_vertices[i]->m_faces)
					if(ea.first->m_selected)
					{
						selected = true; break;
					}
					m_vertices[i]->m_selected = selected;
				}
			}
			//setUndoEnabled(o);
		}

		return true;
	}
	return false;
}
bool Model::unselectGroup(unsigned m)
{
	//LOG_PROFILE(); //???

	if(m>=0&&m<m_groups.size())
//	if(m_groups[m]->m_selected) //2019 //2022: unused?
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionGroups; //2019
			
			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectGroups);
				undo->setSelectionDifference(m,false,true);
				sendUndo(undo);
			}
		}					
		bool o = setUndoEnabled(false);
		for(int i:getGroupTriangles(m))
		{
			m_triangles[i]->m_selected = false;
		}

		selectVerticesFromTriangles(); //OVERKILL?
		
		setUndoEnabled(o);

		m_groups[m]->m_selected = false;

		return true;
	}
	return false;
}
bool Model::unselectBoneJoint(unsigned j)
{
	if(j<m_joints.size()
	&&m_joints[j]->m_selected) //2019
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionJoints; //2019

			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectJoints);
				undo->setSelectionDifference(j,false,true);
				sendUndo(undo);
			}
		}

		m_joints[j]->m_selected = false;

		return true;
	}
	return false;
}
bool Model::unselectPoint(unsigned p)
{
	if(p<m_points.size()
	&&m_points[p]->m_selected) //2019
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionPoints; //2019
		
			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectPoints);
				undo->setSelectionDifference(p,false,true);
				sendUndo(undo);
			}
		}

		m_points[p]->m_selected = false;		
		
		return true;
	}
	return false;
}
bool Model::unselectProjection(unsigned p)
{
	if(p<m_projections.size()
	&&m_projections[p]->m_selected) //2019
	{
		if(!m_selecting) //2020: make foolproof?
		{
			//m_changeBits |= SelectionChange;
			m_changeBits |= SelectionProjections; //2019
		
			if(m_undoEnabled)
			{
				auto undo = new MU_Select(SelectProjections);
				undo->setSelectionDifference(p,false,true);
				sendUndo(undo);
			}
		}

		m_projections[p]->m_selected = false;

		return true;
	}
	return false;
}

bool Model::isVertexSelected(unsigned v)const
{
	if(v>=m_vertices.size())
	return false;
	return m_vertices[v]->m_selected;
}
bool Model::isTriangleSelected(unsigned v)const
{
	if(v>=m_triangles.size())
	return false;
	return m_triangles[v]->m_selected;
}
bool Model::isGroupSelected(unsigned v)const
{
	if(v>=m_groups.size())
	return false;
	return m_groups[v]->m_selected;
}
bool Model::isBoneJointSelected(unsigned j)const
{
	if(j>=m_joints.size())
	return false;
	return m_joints[j]->m_selected;
}
bool Model::isPointSelected(unsigned p)const
{
	if(p>=m_points.size())
	return false;
	return m_points[p]->m_selected;
}
bool Model::isProjectionSelected(unsigned p)const
{
	if(p>=m_projections.size())
	return false;
	return m_projections[p]->m_selected;
}
bool Model::isPositionSelected(Position p)const
{
	switch(p.type)
	{
	case PT_Vertex: return isVertexSelected(p); 
	case PT_Joint: return isBoneJointSelected(p); 
	case PT_Point: return isPointSelected(p); 
	case PT_Projection: return isProjectionSelected(p); 
	}
	return false;
}

bool Model::selectVerticesInVolumeMatrix(bool select, const Matrix &viewMat, double x1, double y1, double x2, double y2,SelectionTest *test)
{
	//LOG_PROFILE(); //???

	//beginSelectionDifference(); //OVERKILL!

	if(x1>x2) std::swap(x1,x2);
	if(y1>y2) std::swap(y1,y2);

	Vector vert;

	for(unsigned v=0;v<m_vertices.size();v++)
	{
		if(m_vertices[v]->m_selected!=select)
		{
			vert.setAll(m_vertices[v]->m_absSource,3);
			
			vert[3] = 1;

			viewMat.apply(vert);

			//TESTING
			//This lets a projection matrix be used to do the selection.
			//I guess it should be a permanent feature.
			double w = vert[3]; if(1!=w) 
			{
				//HACK: Reject if behind Z plane.
				if(w<=0) continue;

				vert.scale(1/w);
			}

			if( m_vertices[v]->m_visible
				  &&vert[0]>=x1&&vert[0]<=x2 
				  &&vert[1]>=y1&&vert[1]<=y2)
			{
				if(test)
					m_vertices[v]->m_selected = test->shouldSelect(m_vertices[v])? select : m_vertices[v]->m_selected;
				else
					m_vertices[v]->m_selected = select;
			}
		}
	}
	
	//endSelectionDifference();

	return true;
}

bool Model::selectTrianglesInVolumeMatrix(bool select, const Matrix &viewMat, double x1, double y1, double x2, double y2, bool connected, SelectionTest *test)
{
	//LOG_PROFILE(); //???

	//beginSelectionDifference(); //OVERKILL!

	unsigned i;
	for(i = 0; i<m_vertices.size(); i++)
	{
		//Note, I think only "connected" uses these?
		m_vertices[i]->m_marked2 = false; 
	}

	for(i = 0; i<m_triangles.size(); i++)
	{
		//These weren't actually used, but is needed
		//for selectGroupsFromTriangles_marked2 now?
		m_triangles[i]->m_marked2 = false; 
	}

	if(x1>x2) std::swap(x1,x2);
	if(y1>y2) std::swap(y1,y2);

	unsigned t;
	for(t = 0; t<m_triangles.size(); t++)
	{
		Triangle *tri = m_triangles[t];
		if(tri->m_selected!=select 
			  &&tri->m_visible
			  &&(!test||(test&&test->shouldSelect(tri))))
		{
			bool above = false;
			bool below = false;

			int v;
			Vertex *vert[3];
			double tCords[3][4];

			// 0. Assign vert to triangle's verticies 
			// 1. Check for vertices within the selection volume in the process
			for(v = 0; v<3; v++)
			{
				//FIX ME
				//Calculating every instance of the vertex is pretty gratuitous.

				unsigned vertId = tri->m_vertexIndices[v];
				vert[v] = m_vertices[vertId];
				
				tCords[v][0] = vert[v]->m_absSource[0]; 
				tCords[v][1] = vert[v]->m_absSource[1]; 
				tCords[v][2] = vert[v]->m_absSource[2]; 
				tCords[v][3] = 1.0;

				viewMat.apply(tCords[v]);				

				//TESTING
				//This lets a projection matrix be used to do the selection.
				//I guess it should be a permanent feature.
				double w = tCords[v][3]; if(1!=w) 
				{
					//HACK: Reject if behind the Z plane because they can't
					//be clipped to the visible/meaningful parts.
					if(w<=0) goto next_triangle;

					((Vector*)tCords[v])->scale(1/w);
				}
			}

			for(v = 0; v<3; v++)
			{
				if( tCords[v][0]>=x1&&tCords[v][0]<=x2 
					  &&tCords[v][1]>=y1&&tCords[v][1]<=y2)
				{
					// A vertex of the triangle is within the selection area
					tri->m_selected = select;

					tri->m_marked2 = true; //selectGroupsFromTriangles_marked2?

					vert[0]->m_marked2 = true;
					vert[1]->m_marked2 = true;
					vert[2]->m_marked2 = true;
					goto next_triangle; // next triangle
				}
			}

			// 2. Find intersections between triangle edges and selection edges
			// 3. Also,check to see if the selection box is completely within triangle

			double m[3];
			double b[3];
			double *coord[3][2];

			m[0] = (tCords[0][1]-tCords[1][1])/(tCords[0][0]-tCords[1][0]);
			coord[0][0] = tCords[0];
			coord[0][1] = tCords[1];
			m[1] = (tCords[0][1]-tCords[2][1])/(tCords[0][0]-tCords[2][0]);
			coord[1][0] = tCords[0];
			coord[1][1] = tCords[2];
			m[2] = (tCords[1][1]-tCords[2][1])/(tCords[1][0]-tCords[2][0]);
			coord[2][0] = tCords[1];
			coord[2][1] = tCords[2];

			b[0] = tCords[0][1]-(m[0] *tCords[0][0]);
			b[1] = tCords[2][1]-(m[1] *tCords[2][0]);
			b[2] = tCords[2][1]-(m[2] *tCords[2][0]);

			for(int line = 0; line<3; line++)
			{
				double y;
				double x;
				double xmin;
				double xmax;
				double ymin;
				double ymax;

				if(coord[line][0][0]<coord[line][1][0])
				{
					xmin = coord[line][0][0];
					xmax = coord[line][1][0];
				}
				else
				{
					xmin = coord[line][1][0];
					xmax = coord[line][0][0];
				}

				if(coord[line][0][1]<coord[line][1][1])
				{
					ymin = coord[line][0][1];
					ymax = coord[line][1][1];
				}
				else
				{
					ymin = coord[line][1][1];
					ymax = coord[line][0][1];
				}

				if(x1>=xmin&&x1<=xmax)
				{
					y = m[line] *x1+b[line];
					if(y>=y1&&y<=y2)
					{
						tri->m_selected = select;

						tri->m_marked2 = true; //selectGroupsFromTriangles_marked2?

						vert[0]->m_marked2 = true;
						vert[1]->m_marked2 = true;
						vert[2]->m_marked2 = true;
						goto next_triangle; // next triangle
					}

					if(y>y1)
					{
						above = true;
					}
					if(y<y1)
					{
						below = true;
					}
				}

				if(x2>=xmin&&x2<=xmax)
				{
					y = m[line] *x2+b[line];
					if(y>=y1&&y<=y2)
					{
						tri->m_selected = select;

						tri->m_marked2 = true; //selectGroupsFromTriangles_marked2?

						vert[0]->m_marked2 = true;
						vert[1]->m_marked2 = true;
						vert[2]->m_marked2 = true;
						goto next_triangle; // next triangle
					}
				}

				if(y1>=ymin&&y1<=ymax)
				{
					if(coord[line][0][0]==coord[line][1][0])
					{
						if(coord[line][0][0]>=x1&&coord[line][0][0]<=x2)
						{
							tri->m_selected = select;

							tri->m_marked2 = true; //selectGroupsFromTriangles_marked2?

							vert[0]->m_marked2 = true;
							vert[1]->m_marked2 = true;
							vert[2]->m_marked2 = true;
							goto next_triangle; // next triangle
						}
					}
					else
					{
						x = (y1-b[line])/m[line];
						if(x>=x1&&x<=x2)
						{
							tri->m_selected = select;

							tri->m_marked2 = true; //selectGroupsFromTriangles_marked2?

							vert[0]->m_marked2 = true;
							vert[1]->m_marked2 = true;
							vert[2]->m_marked2 = true;
							goto next_triangle; // next triangle
						}
					}
				}

				if(y2>=ymin&&y2<=ymax)
				{
					if(coord[line][0][0]==coord[line][1][0])
					{
						if(coord[line][0][0]>=x1&&coord[line][0][0]<=x2)
						{
							tri->m_selected = select;

							tri->m_marked2 = true; //selectGroupsFromTriangles_marked2?

							vert[0]->m_marked2 = true;
							vert[1]->m_marked2 = true;
							vert[2]->m_marked2 = true;
							goto next_triangle; // next triangle
						}
					}
					else
					{
						x = (y2-b[line])/m[line];
						if(x>=x1&&x<=x2)
						{
							tri->m_selected = select;

							tri->m_marked2 = true; //selectGroupsFromTriangles_marked2?

							vert[0]->m_marked2 = true;
							vert[1]->m_marked2 = true;
							vert[2]->m_marked2 = true;
							goto next_triangle; // next triangle
						}
					}
				}
			}

			if(above&&below)
			{
				// There was an intersection above and below the selection area,
				// This means we're inside the triangle, so add it to our selection list
				tri->m_selected = select;

				tri->m_marked2 = true; //selectGroupsFromTriangles_marked2?

				vert[0]->m_marked2 = true;
				vert[1]->m_marked2 = true;
				vert[2]->m_marked2 = true;
				goto next_triangle; // next triangle
			}

next_triangle:
			; // because we need a statement after a label
		}
	}

	if(connected)
	{
		bool found = true; while(found)
		{
			found = false;
			for(auto*tri:m_triangles) if(tri->m_visible)
			{
				int count = 0;
				for(unsigned v = 0; v<3; v++)
				{
					if(m_vertices[tri->m_vertexIndices[v]]->m_marked2)
					{
						count++;
					}
				}

				if(count>0&&(count<3||tri->m_selected!=select))
				{
					found = true;

					tri->m_selected = select;

					tri->m_marked2 = true; //selectGroupsFromTriangles_marked2?

					for(unsigned v = 0; v<3; v++)
					{
						m_vertices[tri->m_vertexIndices[v]]->m_marked2 = true;
					}
				}
			}
		}
	}

	selectVerticesFromTriangles(); //OVERKILL? WHY NOT SET IN THE ABOVE LOGIC?

	//endSelectionDifference();

	return true;
}

bool Model::selectGroupsInVolumeMatrix(bool select, const Matrix &viewMat, double x1, double y1, double x2, double y2, SelectionTest *test)
{
	//LOG_PROFILE(); //???

	//beginSelectionDifference(); //OVERKILL!

	selectTrianglesInVolumeMatrix(select,viewMat,x1,y1,x2,y2,false,test);

	//selectGroupsFromTriangles(!select);
	selectGroupsFromTriangles_marked2(select);

	//endSelectionDifference();

	return true;
}

bool Model::selectBoneJointsInVolumeMatrix(bool select, const Matrix &viewMat, double x1, double y1, double x2, double y2, SelectionTest *test)
{
	//LOG_PROFILE(); //???

	//beginSelectionDifference(); //OVERKILL!

	if(x1>x2) std::swap(x1,x2);
	if(y1>y2) std::swap(y1,y2);

	Vector vec;

	for(unsigned j = 0; j<m_joints.size(); j++)
	{
		Joint *joint = m_joints[j];

		if(joint->m_selected!=select&&joint->m_visible)
		{
			//TODO? Need to update m_final matrix.
			//validateAnimSkel();

			vec[0] = joint->m_final.get(3,0);
			vec[1] = joint->m_final.get(3,1);
			vec[2] = joint->m_final.get(3,2);
			vec[3] = 1.0;

			viewMat.apply(vec);

			//TESTING
			//This lets a projection matrix be used to do the selection.
			//I guess it should be a permanent feature.
			double w = vec[3]; if(1!=w) 
			{
				//HACK: Reject if behind Z plane.
				if(w<=0) continue;

				vec.scale(1/w);
			}

			if(vec[0]>=x1&&vec[0]<=x2 
				  &&vec[1]>=y1&&vec[1]<=y2)
			{
				if(test)
					joint->m_selected = test->shouldSelect(joint)? select : joint->m_selected;
				else
					joint->m_selected = select;
			}
		}
	}

	//endSelectionDifference();

	return true;
}

bool Model::selectPointsInVolumeMatrix(bool select, const Matrix &viewMat, double x1, double y1, double x2, double y2,SelectionTest *test)
{
	//LOG_PROFILE(); //???

	//beginSelectionDifference(); //OVERKILL!

	if(x1>x2) std::swap(x1,x2);
	if(y1>y2) std::swap(y1,y2);

	Vector vec;

	for(unsigned p = 0; p<m_points.size(); p++)
	{
		Point *point = m_points[p];

		if(point->m_selected!=select&&point->m_visible)
		{
			vec.setAll(point->m_absSource,3);
			vec[3] = 1.0;

			viewMat.apply(vec);

			//TESTING
			//This lets a projection matrix be used to do the selection.
			//I guess it should be a permanent feature.
			double w = vec[3]; if(1!=w) 
			{
				//HACK: Reject if behind Z plane.
				if(w<=0) continue;

				vec.scale(1/w);
			}

			if(vec[0]>=x1&&vec[0]<=x2 
				  &&vec[1]>=y1&&vec[1]<=y2)
			{
				if(test)
					point->m_selected = test->shouldSelect(point)? select : point->m_selected;
				else
					point->m_selected = select;
			}
		}
	}
	//endSelectionDifference();

	return true;
}

bool Model::selectProjectionsInVolumeMatrix(bool select, const Matrix &viewMat, double x1, double y1, double x2, double y2,SelectionTest *test)
{
	//LOG_PROFILE(); //???

	//beginSelectionDifference(); //OVERKILL!

	if(x1>x2) std::swap(x1,x2);
	if(y1>y2) std::swap(y1,y2);

	if(m_animationMode==ANIMMODE_NONE)
	{
		for(unsigned p = 0; p<m_projections.size(); p++)
		{
			TextureProjection *proj = m_projections[p];

			if(proj->m_selected!=select)
			{
				Matrix m = proj->getMatrixUnanimated();
				auto &left = *(Vector*)m.getVector(0);
				auto &up = *(Vector*)m.getVector(1);
				//auto &seam = *(Vector*)m.getVector(2);
				auto &pos = *(Vector*)m.getVector(3);

				viewMat.apply(pos);
				//TESTING
				//This lets a projection matrix be used to do the selection.
				//I guess it should be a permanent feature.				
				double w = pos[3]; if(1!=w) 
				{
					//HACK: Reject if behind Z plane.
					if(w<=0) continue;

					pos.scale(1/w);
				}
				viewMat.apply3(up);
				//viewMat.apply3(seam);
				viewMat.apply3(left);

				bool selectable = false;

				if(proj->m_type==TPT_Plane)
				{
					bool above = false;
					bool below = false;

					int v;
					double tCords[4][3];

					// 0. Assign vert to triangle's verticies 
					// 1. Check for vertices within the selection volume in the process
					for(v = 0; v<4; v++)
					{
						double x = (v==0||v==3)? -1.0 : 1.0;
						double y = (v>=2)? -1.0 : 1.0;

						tCords[v][0] = pos[0]+up[0] *y+left[0] *x;
						tCords[v][1] = pos[1]+up[1] *y+left[1] *x;
						tCords[v][2] = pos[2]+up[2] *y+left[2] *x;

						//log_debug("vertex %d: %f,%f,%f\n",v,tCords[v][0],tCords[v][1],tCords[v][2]); //???
					}

					for(v = 0; v<4; v++)
					{
						if( tCords[v][0]>=x1&&tCords[v][0]<=x2 
							  &&tCords[v][1]>=y1&&tCords[v][1]<=y2)
						{
							// A vertex of the square is within the selection area
							selectable = true;
						}

						//log_debug("xform: %d: %f,%f,%f\n",v,tCords[v][0],tCords[v][1],tCords[v][2]); //???
					}

					// 2. Find intersections between triangle edges and selection edges
					// 3. Also,check to see if the selection box is completely within triangle

					double m[4];
					double b[4];
					double *coord[4][2];

					m[0] = (tCords[0][1]-tCords[1][1])/(tCords[0][0]-tCords[1][0]);
					coord[0][0] = tCords[0];
					coord[0][1] = tCords[1];
					m[1] = (tCords[1][1]-tCords[2][1])/(tCords[1][0]-tCords[2][0]);
					coord[1][0] = tCords[1];
					coord[1][1] = tCords[2];
					m[2] = (tCords[2][1]-tCords[3][1])/(tCords[2][0]-tCords[3][0]);
					coord[2][0] = tCords[2];
					coord[2][1] = tCords[3];
					m[3] = (tCords[3][1]-tCords[0][1])/(tCords[3][0]-tCords[0][0]);
					coord[3][0] = tCords[3];
					coord[3][1] = tCords[0];

					b[0] = tCords[0][1]-(m[0] *tCords[0][0]);
					b[1] = tCords[1][1]-(m[1] *tCords[1][0]);
					b[2] = tCords[2][1]-(m[2] *tCords[2][0]);
					b[3] = tCords[3][1]-(m[3] *tCords[3][0]);


					for(int line = 0; !selectable&&line<4; line++)
					{
						log_debug("line %d:	m = %f	b = %f	x = %f	y = %f\n",
								line,m[line],b[line],coord[line][0][0],coord[line][0][1]);
						double y;
						double x;
						double xmin;
						double xmax;
						double ymin;
						double ymax;

						if(coord[line][0][0]<coord[line][1][0])
						{
							xmin = coord[line][0][0];
							xmax = coord[line][1][0];
						}
						else
						{
							xmin = coord[line][1][0];
							xmax = coord[line][0][0];
						}

						if(coord[line][0][1]<coord[line][1][1])
						{
							ymin = coord[line][0][1];
							ymax = coord[line][1][1];
						}
						else
						{
							ymin = coord[line][1][1];
							ymax = coord[line][0][1];
						}

						if(x1>=xmin&&x1<=xmax)
						{
							y = m[line] *x1+b[line];
							if(y>=y1&&y<=y2)
							{
								selectable = true;
							}

							if(y>y1)
							{
								above = true;
							}
							if(y<y1)
							{
								below = true;
							}
						}

						if(!selectable&&x2>=xmin&&x2<=xmax)
						{
							y = m[line] *x2+b[line];
							if(y>=y1&&y<=y2)
							{
								selectable = true;
							}
						}

						bool vertical = (fabs(coord[line][0][0]-coord[line][1][0])<0.0001);
						if(!selectable&&y1>=ymin&&y1<=ymax)
						{
							if(vertical)
							{
								if(coord[line][0][0]>=x1&&coord[line][0][0]<=x2)
								{
									selectable = true;
								}
							}
							else
							{
								x = (y1-b[line])/m[line];
								if(x>=x1&&x<=x2)
								{
									selectable = true;
								}
							}
						}

						if(!selectable&&y2>=ymin&&y2<=ymax)
						{
							if(vertical)
							{
								if(coord[line][0][0]>=x1&&coord[line][0][0]<=x2)
								{
									selectable = true;
								}
							}
							else
							{
								x = (y2-b[line])/m[line];
								if(x>=x1&&x<=x2)
								{
									selectable = true;
								}
							}
						}
					}

					if(above&&below)
					{
						// There was an intersection above and below the selection area,
						// This means we're inside the square,so add it to our selection list
						selectable = true;
					}
				}
				else
				{
					int i = 0;
					int iMax = 0;

					double radius = up.mag3();

					double diffX = up[0];
					double diffY = up[1];

					if(proj->m_type==TPT_Cylinder)
					{
						radius = radius/3;

						i	 = -1;
						iMax =  1;
					}

					for(; i<=iMax&&!selectable; i++)
					{
						double x = pos[0];
						double y = pos[1];

						x += diffX *(double)i;
						y += diffY *(double)i;

						// check if center is inside selection region
						bool inx = (x>=x1&&x<=x2);
						bool iny = (y>=y1&&y<=y2);

						selectable = (inx&&iny);

						if(!selectable)
						{
							// check if lines passes through radius
							if(inx)
							{
								if(fabs(y-y1)<radius
									  ||fabs(y-y2)<radius)
								{
									selectable = true;
								}
							}
							else if(iny)
							{
								if(fabs(x-x1)<radius
									  ||fabs(x-x2)<radius)
								{
									selectable = true;
								}
							}
							else
							{
								// line did not pass through radius,see if all bounding region 
								// points are within radius
								double diff[2];

								diff[0] = fabs(x-x1);
								diff[1] = fabs(y-y1);

								if(sqrt(diff[0]*diff[0]+diff[1]*diff[1])<radius)
								{
									selectable = true;
								}

								diff[0] = fabs(x-x2);
								diff[1] = fabs(y-y1);

								if(sqrt(diff[0]*diff[0]+diff[1]*diff[1])<radius)
								{
									selectable = true;
								}

								diff[0] = fabs(x-x1);
								diff[1] = fabs(y-y2);

								if(sqrt(diff[0]*diff[0]+diff[1]*diff[1])<radius)
								{
									selectable = true;
								}

								diff[0] = fabs(x-x2);
								diff[1] = fabs(y-y2);

								if(sqrt(diff[0]*diff[0]+diff[1]*diff[1])<radius)
								{
									selectable = true;
								}
							}
						}
					}
				}

				if(selectable)
				{
					if(test)
						proj->m_selected = test->shouldSelect(proj)? select : proj->m_selected;
					else
						proj->m_selected = select;
				}
			}
		}
	}

	//endSelectionDifference();

	return true;
}

bool Model::selectInVolumeMatrix(const Matrix &viewMat, double x1, double y1, double x2, double y2,SelectionTest *test)
{
	//LOG_PROFILE(); //???

	bool selecting = m_selecting; //2020

	if(!selecting) beginSelectionDifference(); //OVERKILL!

	switch (m_selectionMode)
	{
	case SelectVertices:
		return selectVerticesInVolumeMatrix (true,viewMat,x1,y1,x2,y2,test);
		break;
	case SelectTriangles:
		return selectTrianglesInVolumeMatrix (true,viewMat,x1,y1,x2,y2,false,test);
		break;
	case SelectConnected:
		return selectTrianglesInVolumeMatrix (true,viewMat,x1,y1,x2,y2,true,test);
		break;
	case SelectGroups:
		return selectGroupsInVolumeMatrix (true,viewMat,x1,y1,x2,y2,test);
		break;
	case SelectJoints:
		return selectBoneJointsInVolumeMatrix (true,viewMat,x1,y1,x2,y2,test);
		break;
	case SelectPoints:
		return selectPointsInVolumeMatrix (true,viewMat,x1,y1,x2,y2,test);
		break;
	case SelectProjections:
		return selectProjectionsInVolumeMatrix (true,viewMat,x1,y1,x2,y2,test);
		break;
	default:
		break; //???
	}

	if(!selecting) endSelectionDifference(); //2020

	return true; //???
}

bool Model::unselectInVolumeMatrix(const Matrix &viewMat, double x1, double y1, double x2, double y2,SelectionTest *test)
{
	//LOG_PROFILE(); //???

	bool selecting = m_selecting; //2020

	if(!selecting) beginSelectionDifference(); //OVERKILL!

	switch (m_selectionMode)
	{
	case SelectVertices:
		return selectVerticesInVolumeMatrix (false,viewMat,x1,y1,x2,y2,test);
		break;
	case SelectTriangles:
		return selectTrianglesInVolumeMatrix (false,viewMat,x1,y1,x2,y2,false,test);
		break;
	case SelectConnected:
		return selectTrianglesInVolumeMatrix (false,viewMat,x1,y1,x2,y2,true,test);
		break;
	case SelectGroups:
		return selectGroupsInVolumeMatrix (false,viewMat,x1,y1,x2,y2,test);
		break;
	case SelectJoints:
		return selectBoneJointsInVolumeMatrix (false,viewMat,x1,y1,x2,y2,test);
		break;
	case SelectPoints:
		return selectPointsInVolumeMatrix (false,viewMat,x1,y1,x2,y2,test);
		break;
	case SelectProjections:
		return selectProjectionsInVolumeMatrix (false,viewMat,x1,y1,x2,y2,test);
		break;
	default:
		break; //???
	}

	if(!selecting) endSelectionDifference(); //2020

	return true; //???
}

void Model::selectVerticesFromTriangles()
{
	//LOG_PROFILE(); //???

	//2019: Trying to cover all bases.
	//NOTE: MU_Select calls this to unselect triangles on undo/redo.
	//It was doing so for every triangle.
	//if(!m_selecting) m_changeBits |= SelectionChange|SelectionVertices;
	if(!m_selecting) m_changeBits|=SelectionVertices;

	unselectAllVertices(); //???

	assert(m_selecting||!m_undoEnabled); //BELOW CODE IS NOT UNDOABLE

	for(auto*ea:m_triangles) if(ea->m_selected)
	{
		//NOTE/REMINDER: Hidden vertices need to be selected as well
		//owing to how the Delete function is implemented at present.

		for(int v=3;v-->0;)
		m_vertices[ea->m_vertexIndices[v]]->m_selected = true;
	}
}

//bool Model::selectTrianglesFromGroups() 
bool Model::selectTrianglesFromGroups_marked(bool how) 
{
	//LOG_PROFILE(); //???

	//2022: Shouldn't selected triangles remain intact? Yes, seems
	//this causes seome weird side effects with the Group selection
	//tool.
	//unselectAllTriangles();

	assert(m_selecting||!m_undoEnabled); //BELOW CODE IS NOT UNDOABLE

	bool sel = false;

	for(auto*grp:m_groups)
	{
		//if(grp->m_selected) //selectGroupsFromTriangles_marked2?
		if(grp->m_marked)
		{
			for(auto i:grp->m_triangleIndices)
			{
				auto *tp = m_triangles[i];

				//if(tp->m_visible&&!tp->m_selected)
				if(tp->m_visible&&how!=tp->m_selected)
				{
					sel = true; tp->m_selected = how;
				}
			}
		}
	}

	//2022: maybe this shouldn't be here but it seems
	//harmless and I worry callers aren't tracking it.
	if(sel&&!m_selecting) m_changeBits|=SelectionFaces;

	if(sel) selectVerticesFromTriangles(); //OVERKILL?

	return sel;
}

bool Model::selectUngroupedTriangles(bool how) //2022
{
	//https://github.com/zturtleman/mm3d/issues/90
	//if(m_undoEnabled) beginSelectionDifference(); //OVERKILL!

	MU_Select *undo = nullptr; //2020

	bool sel = false;

	for(unsigned t=m_triangles.size();t-->0;)
	{
		auto *tp = m_triangles[t];

		if(tp->m_group!=-1) continue;

		if(how==tp->m_selected) continue; //2020

		sel = true;

		if(!m_selecting) //2020: making fullproof?
		{
			if(m_undoEnabled)
			{
				if(!undo) undo = new MU_Select(SelectTriangles);

				undo->setSelectionDifference(t,how,!how);
			}
		}

		tp->m_selected = how;
	}

	if(sel&&!m_selecting) m_changeBits|=SelectionFaces; //2020
	
	sendUndo(undo); return sel;
}

void Model::selectTrianglesFromVertices(bool all)
{
	//LOG_PROFILE(); //???

	unselectAllTriangles();

	assert(m_selecting||!m_undoEnabled); //BELOW CODE IS NOT UNDOABLE

	for(unsigned t = 0; t<m_triangles.size(); t++)
	{
		if(m_triangles[t]->m_visible)
		{
			int count = 0;
			for(int v = 0; v<3; v++)
			{
				if(m_vertices[m_triangles[t]->m_vertexIndices[v]]->m_selected)
				{
					count++;
				}

			}
			if(all)
			{
				if(count==3)
				{
					m_triangles[t]->m_selected = true;
				}
			}
			else
			{
				if(count>0)
				{
					m_triangles[t]->m_selected = true;
				}
			}
		}
	}

	// Unselect vertices who don't have a triangle selected
	if(all)
	{
		unselectAllVertices();
		selectVerticesFromTriangles();
	}
}

//void Model::selectGroupsFromTriangles(bool all)
void Model::selectGroupsFromTriangles_marked2(bool how)
{
	//LOG_PROFILE(); //???

	//unselectAllGroups(); //2022: WHAT? 

	assert(m_selecting||!m_undoEnabled); //BELOW CODE IS NOT UNDOABLE

	for(auto*grp:m_groups)
	{
		unsigned count = 0;
		for(auto i:grp->m_triangleIndices)
		{
			//2022: only selectGroupsInVolumeMatrix has a legit reason
			//to use this, and it sets m_marked2, so this way it doesn't
			//accidentally inadvertently select unselected groups
			//if(m_triangles[i]->m_selected) count++;
			if(m_triangles[i]->m_marked2){ count++; break; }
		}
		//grp->m_selected = //same
		//all?count==grp->m_triangleIndices.size():count!=0;
		grp->m_marked = count!=0;		
	}

	// Unselect vertices who don't have a triangle selected
	// 
	//First of all selectTrianglesFromGroups already did this
	//but it shouldn't have
	//unselectAllTriangles();
	//selectTrianglesFromGroups();
	selectTrianglesFromGroups_marked(how); //REMOVE ME
}

bool Model::invertSelection()
{
	//LOG_PROFILE(); //???

	beginSelectionDifference(); //OVERKILL!
	switch (m_selectionMode)
	{
		case SelectVertices:
			for(unsigned v = 0; v<m_vertices.size(); v++)
			{
				if(m_vertices[v]->m_visible)
				{
					m_vertices[v]->m_selected = m_vertices[v]->m_selected ? false : true;
				}
			}
			break;
			
		case SelectGroups:
			/*UNNECESSARY (SILLY)
			for(unsigned g = 0; g<m_groups.size(); g++)
			{
				m_groups[g]->m_selected = m_groups[g]->m_selected ? false : true;
			}
			selectTrianglesFromGroups(); //CAN'T ABIDE THIS
			break;*/
		case SelectConnected: //2020
		case SelectTriangles:
			for(unsigned t = 0; t<m_triangles.size(); t++)
			{
				if(m_triangles[t]->m_visible)
				{
					m_triangles[t]->m_selected = m_triangles[t]->m_selected ? false : true;
				}
			}
			selectVerticesFromTriangles();
			break;

		case SelectJoints:
			for(unsigned j = 0; j<m_joints.size(); j++)
			{
				m_joints[j]->m_selected = m_joints[j]->m_selected ? false : true;
			}
			break;

		case SelectPoints:
			for(unsigned p = 0; p<m_points.size(); p++)
			{
				m_points[p]->m_selected = m_points[p]->m_selected ? false : true;
			}
			break;

		default:
			break;
	}
	endSelectionDifference();

	return true;
}

void Model::beginSelectionDifference()
{
	//LOG_PROFILE(); //???

	if(!m_selecting)
	{
		m_selecting = true;

		//Wait until endSelectionDifference?
		//m_changeBits |= SelectionChange; //2019

		//REMOVE ME? (SEEMS VERY WORK INTENSIVE FOR VERY LITTE GAIN)
		if(m_undoEnabled)
		{
			//SHOULD THIS BE LIMITED TO m_selectionMode?
			unsigned t;
			for(t = 0; t<m_vertices.size(); t++)
			{
				m_vertices[t]->m_marked = m_vertices[t]->m_selected;
			}
			for(t = 0; t<m_triangles.size(); t++)
			{
				m_triangles[t]->m_marked = m_triangles[t]->m_selected;
			}
			for(t = 0; t<m_groups.size(); t++)
			{
				m_groups[t]->m_marked = m_groups[t]->m_selected;
			}
			for(t = 0; t<m_joints.size(); t++)
			{
				m_joints[t]->m_marked = m_joints[t]->m_selected;
			}
			for(t = 0; t<m_points.size(); t++)
			{
				m_points[t]->m_marked = m_points[t]->m_selected;
			}
			for(t = 0; t<m_projections.size(); t++)
			{
				m_projections[t]->m_marked = m_projections[t]->m_selected;
			}
		}
	}
}

namespace
{
	template<class T> struct model_select
	{
		MU_Select *undo;
		operator MU_Select*(){ return undo; }
		model_select(bool ue, int &cb, std::vector<T*> &l, Model::SelectionModeE e, Model::ChangeBits f):undo()
		{
			int i,iN = (unsigned)l.size();
			for(i=0;i<iN;i++)			
			if(l[i]->m_selected!=l[i]->m_marked)
			break;
			if(i!=iN)
			{			
				cb|=f; if(!ue) return;

				undo = new MU_Select(e);
				for(;i<iN;i++)			
				if(l[i]->m_selected!=l[i]->m_marked)
				undo->setSelectionDifference(i,l[i]->m_selected,l[i]->m_marked);
				//sendUndo(undo);
			}
		}
	};
}
void Model::endSelectionDifference()
{	
	if(!m_selecting) return; //2020

	//LOG_PROFILE(); //???
	
	m_selecting = false;

	bool ue = m_undoEnabled;
	//FIX ME
	//These enum names are too similar. (This was a bug immediately after implementing model_select.)
	sendUndo(model_select<Vertex>(ue,m_changeBits,m_vertices,SelectVertices,SelectionVertices)/*,true*/);
	sendUndo(model_select<Triangle>(ue,m_changeBits,m_triangles,SelectTriangles,SelectionFaces)/*,true*/);
	sendUndo(model_select<Group>(ue,m_changeBits,m_groups,SelectGroups,SelectionGroups)/*,true*/);
	sendUndo(model_select<Joint>(ue,m_changeBits,m_joints,SelectJoints,SelectionJoints)/*,true*/);
	sendUndo(model_select<Point>(ue,m_changeBits,m_points,SelectPoints,SelectionPoints)/*,true*/);
	sendUndo(model_select<TextureProjection>(ue,m_changeBits,m_projections,SelectProjections,SelectionProjections)/*,true*/);
}

void Model::getSelectedPositions(pos_list &positions)const
{
	unsigned count,t;

	positions.clear();
	
	//DUPLICATE
	//getSelectedBoneJoints use m_joints2 now
	count = m_joints.size();
	//for(t=0;t<count;t++)	
	for(auto&ea:m_joints2)
	if(ea.second->m_selected)
	{
		Position p;
		p.type = PT_Joint;
		//p.index = t;
		p.index = ea.first;
		positions.push_back(p);
	}

	count = m_points.size();
	for(t=0;t<count;t++)
	if(m_points[t]->m_selected)
	{
		Position p;
		p.type = PT_Point;
		p.index = t;
		positions.push_back(p);
	}

	count = m_projections.size();
	for(t=0;t<count;t++)
	if(m_projections[t]->m_selected)
	{
		Position p;
		p.type = PT_Projection;
		p.index = t;
		positions.push_back(p);
	}

	//2020: Making PT_Vertex last in list since there
	//are likely to be many vertices, so iteration is
	//able to get at the non-vertex data and skip the
	//rest without using backward iteration semantics.
	count = m_vertices.size();
	for(t=0;t<count;t++)	
	if(m_vertices[t]->m_selected)
	{
		Position p;
		p.type = PT_Vertex;
		p.index = t;
		positions.push_back(p);
	}	
}

void Model::getSelectedVertices(int_list &vertices)const
{
	vertices.clear();

	for(unsigned t = 0; t<m_vertices.size(); t++)
	{
		if(m_vertices[t]->m_selected)
		{
			vertices.push_back(t);
		}
	}
}

void Model::getSelectedTriangles(int_list &triangles)const
{
	triangles.clear();

	for(unsigned t = 0; t<m_triangles.size(); t++)
	{
		if(m_triangles[t]->m_selected)
		{
			triangles.push_back(t);
		}
	}
}

void Model::getSelectedGroups(int_list &groups)const
{
	groups.clear();

	for(unsigned t = 0; t<m_groups.size(); t++)
	{
		if(m_groups[t]->m_selected)
		{
			groups.push_back(t);
		}
	}
}

void Model::getSelectedBoneJoints(int_list &joints)const
{
	joints.clear();

	//FIXING copySelected
	//This way some algorithms don't have to be rewritten
	//to be order independent but it's not very transparent.
	//DUPLICATE: getSelectedPositions should reflect this too.
	//for(unsigned t=0;t<m_joints.size();t++)
	for(auto&ea:m_joints2)
	{
		//if(m_joints[t]->m_selected)
		if(ea.second->m_selected)
		{
			joints.push_back(ea.first); //t
		}
	}
}

void Model::getSelectedPoints(int_list &points)const
{
	points.clear();

	for(unsigned t = 0; t<m_points.size(); t++)
	{
		if(m_points[t]->m_selected)
		{
			points.push_back(t);
		}
	}
}

void Model::getSelectedProjections(int_list &projections)const
{
	projections.clear();

	for(unsigned t = 0; t<m_projections.size(); t++)
	{
		if(m_projections[t]->m_selected)
		{
			projections.push_back(t);
		}
	}
}

unsigned Model::getSelectedVertexCount()const
{
	unsigned c = m_vertices.size();
	unsigned count = 0;

	for(unsigned v = 0; v<c; v++)
	{
		if(m_vertices[v]->m_selected)
		{
			count++;
		}
	}

	return count;
}

unsigned Model::getSelectedTriangleCount()const
{
	unsigned c = m_triangles.size();
	unsigned count = 0;

	for(unsigned v = 0; v<c; v++)
	{
		if(m_triangles[v]->m_selected)
		{
			count++;
		}
	}

	return count;
}

unsigned Model::getSelectedGroupCount()const
{
	unsigned c = m_groups.size();
	unsigned count = 0;

	for(unsigned v = 0; v<c; v++)
	{
		if(m_groups[v]->m_selected)
		{
			count++;
		}
	}

	return count;
}

unsigned Model::getSelectedBoneJointCount()const
{
	unsigned c = m_joints.size();
	unsigned count = 0;

	for(unsigned v = 0; v<c; v++)
	{
		if(m_joints[v]->m_selected)
		{
			count++;
		}
	}

	return count;
}

unsigned Model::getSelectedPointCount()const
{
	unsigned c = m_points.size();
	unsigned count = 0;

	for(unsigned v = 0; v<c; v++)
	{
		if(m_points[v]->m_selected)
		{
			count++;
		}
	}

	return count;
}

unsigned Model::getSelectedProjectionCount()const
{
	unsigned c = m_projections.size();
	unsigned count = 0;

	for(unsigned v = 0; v<c; v++)
	{
		if(m_projections[v]->m_selected)
		{
			count++;
		}
	}

	return count;
}

bool Model::parentJointSelected(int joint)const
{
	while(m_joints[joint]->m_parent>=0)
	{
		joint = m_joints[joint]->m_parent;

		if(m_joints[joint]->m_selected)
		{
			return true;
		}
	}
	return false;
}

bool Model::directParentJointSelected(int joint)const
{
	int p = m_joints[joint]->m_parent;
	return p>=0&&m_joints[p]->m_selected;
}

bool Model::getSelectedBoundingRegion(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2)const
{
	//REMOVE ME: Duplicate of getBoundingRegion

	if(!x1||!y1||!z1||!x2||!y2||!z2) return false; //???
	
	validateAnim(); //2020

	int visible = 0;
	bool havePoint = false; //REMOVE ME
	*x1 = *y1 = *z1 = *x2 = *y2 = *z2 = 0.0;

	for(unsigned v = 0; v<m_vertices.size(); v++)
	{
		if(m_vertices[v]->m_visible&&m_vertices[v]->m_selected)
		{
			if(havePoint) //???
			{
				if(m_vertices[v]->m_absSource[0]<*x1)
				{
					*x1 = m_vertices[v]->m_absSource[0];
				}
				if(m_vertices[v]->m_absSource[0]>*x2)
				{
					*x2 = m_vertices[v]->m_absSource[0];
				}
				if(m_vertices[v]->m_absSource[1]<*y1)
				{
					*y1 = m_vertices[v]->m_absSource[1];
				}
				if(m_vertices[v]->m_absSource[1]>*y2)
				{
					*y2 = m_vertices[v]->m_absSource[1];
				}
				if(m_vertices[v]->m_absSource[2]<*z1)
				{
					*z1 = m_vertices[v]->m_absSource[2];
				}
				if(m_vertices[v]->m_absSource[2]>*z2)
				{
					*z2 = m_vertices[v]->m_absSource[2];
				}
			}
			else //???
			{
				*x1 = *x2 = m_vertices[v]->m_absSource[0];
				*y1 = *y2 = m_vertices[v]->m_absSource[1];
				*z1 = *z2 = m_vertices[v]->m_absSource[2];
				havePoint = true;
			}
			visible++;
		}
	}

	for(unsigned j = 0; j<m_joints.size(); j++)
	{
		if(m_joints[j]->m_selected)
		{
			double coord[3];
			m_joints[j]->m_final.getTranslation(coord);

			if(havePoint) //???
			{
				if(coord[0]<*x1)
				{
					*x1 = coord[0];
				}
				if(coord[0]>*x2)
				{
					*x2 = coord[0];
				}
				if(coord[1]<*y1)
				{
					*y1 = coord[1];
				}
				if(coord[1]>*y2)
				{
					*y2 = coord[1];
				}
				if(coord[2]<*z1)
				{
					*z1 = coord[2];
				}
				if(coord[2]>*z2)
				{
					*z2 = coord[2];
				}
			}
			else //???
			{
				*x1 = *x2 = coord[0];
				*y1 = *y2 = coord[1];
				*z1 = *z2 = coord[2];
				havePoint = true;
			}

			visible++;
		}
	}

	for(unsigned p = 0; p<m_points.size(); p++)
	{
		if(m_points[p]->m_selected)
		{
			double coord[3];
			coord[0] = m_points[p]->m_absSource[0];
			coord[1] = m_points[p]->m_absSource[1];
			coord[2] = m_points[p]->m_absSource[2];

			if(havePoint) //???
			{
				if(coord[0]<*x1)
				{
					*x1 = coord[0];
				}
				if(coord[0]>*x2)
				{
					*x2 = coord[0];
				}
				if(coord[1]<*y1)
				{
					*y1 = coord[1];
				}
				if(coord[1]>*y2)
				{
					*y2 = coord[1];
				}
				if(coord[2]<*z1)
				{
					*z1 = coord[2];
				}
				if(coord[2]>*z2)
				{
					*z2 = coord[2];
				}
			}
			else //???
			{
				*x1 = *x2 = coord[0];
				*y1 = *y2 = coord[1];
				*z1 = *z2 = coord[2];
				havePoint = true;
			}

			visible++;
		}
	}

	for(unsigned p = 0; p<m_projections.size(); p++)
	{
		if(m_projections[p]->m_selected)
		{
			double coord[3];
			//double scale = mag3(m_projections[p]->m_upVec);
			double scale = m_projections[p]->m_xyz[0];
			double *pos = m_projections[p]->m_abs;

			coord[0] = pos[0]+scale;
			coord[1] = pos[1]+scale;
			coord[2] = pos[2]+scale;

			if(havePoint) //???
			{
				if(coord[0]<*x1)
				{
					*x1 = coord[0];
				}
				if(coord[0]>*x2)
				{
					*x2 = coord[0];
				}
				if(coord[1]<*y1)
				{
					*y1 = coord[1];
				}
				if(coord[1]>*y2)
				{
					*y2 = coord[1];
				}
				if(coord[2]<*z1)
				{
					*z1 = coord[2];
				}
				if(coord[2]>*z2)
				{
					*z2 = coord[2];
				}
			}
			else
			{
				*x1 = *x2 = coord[0];
				*y1 = *y2 = coord[1];
				*z1 = *z2 = coord[2];
				havePoint = true;
			}

			coord[0] = pos[0]-scale;
			coord[1] = pos[1]-scale;
			coord[2] = pos[2]-scale;

			if(coord[0]<*x1)
			{
				*x1 = coord[0];
			}
			if(coord[0]>*x2)
			{
				*x2 = coord[0];
			}
			if(coord[1]<*y1)
			{
				*y1 = coord[1];
			}
			if(coord[1]>*y2)
			{
				*y2 = coord[1];
			}
			if(coord[2]<*z1)
			{
				*z1 = coord[2];
			}
			if(coord[2]>*z2)
			{
				*z2 = coord[2];
			}

			visible++;
		}
	}

	return visible!=0;
}

bool Model::selectAllVertices(bool how)
{
	//https://github.com/zturtleman/mm3d/issues/90
	//if(m_undoEnabled) beginSelectionDifference(); //OVERKILL!

	MU_Select *undo = nullptr; //2020

	bool ret = false;

	for(unsigned v=0;v<m_vertices.size();v++)
	{
		if(how==m_vertices[v]->m_selected) continue; //2020

		ret = true;

		if(!m_selecting) //2020: making fullproof?
		{
			if(m_undoEnabled)
			{
				if(!undo) undo = new MU_Select(SelectVertices);

				undo->setSelectionDifference(v,how,!how);
			}
		}

		m_vertices[v]->m_selected = how;
	}

	if(ret&&!m_selecting) m_changeBits|=SelectionVertices; //2020

	sendUndo(undo); return true;
}
bool Model::selectAllTriangles(bool how)
{
	//https://github.com/zturtleman/mm3d/issues/90
	//if(m_undoEnabled) beginSelectionDifference(); //OVERKILL!

	MU_Select *undo = nullptr; //2020

	bool ret = false;

	for(unsigned t=m_triangles.size();t-->0;)
	{
		if(how==m_triangles[t]->m_selected) continue; //2020

		ret = true;

		if(!m_selecting) //2020: making fullproof?
		{
			if(m_undoEnabled)
			{
				if(!undo) undo = new MU_Select(SelectTriangles);

				undo->setSelectionDifference(t,how,!how);
			}
		}

		m_triangles[t]->m_selected = how;
	}

	if(ret&&!m_selecting) m_changeBits|=SelectionFaces; //2020
	
	sendUndo(undo); return true;
}
bool Model::selectAllGroups(bool how)
{
	//https://github.com/zturtleman/mm3d/issues/90
	//if(m_undoEnabled) beginSelectionDifference(); //OVERKILL!

	MU_Select *undo = nullptr; //2020

	bool ret = false;

	for(unsigned m = 0; m<m_groups.size(); m++)
	{
		if(how==m_groups[m]->m_selected) continue; //2020

		ret = true;

		if(!m_selecting) //2020: making fullproof?
		{
			if(m_undoEnabled)
			{
				if(!undo) undo = new MU_Select(SelectGroups);

				undo->setSelectionDifference(m,how,!how);
			}
		}

		m_groups[m]->m_selected = how; //???
	}

	if(ret&&!m_selecting) m_changeBits|=SelectionGroups; //2020

	sendUndo(undo); return true;
}
bool Model::selectAllBoneJoints(bool how)
{
	//https://github.com/zturtleman/mm3d/issues/90
	//if(m_undoEnabled) beginSelectionDifference(); //OVERKILL!

	bool ret = false;

	MU_Select *undo = nullptr; //2020

	for(unsigned j = 0; j<m_joints.size(); j++)
	{
		if(how==m_joints[j]->m_selected) continue; //2020
		
		ret = true;

		if(!m_selecting) //2020: making fullproof?
		{
			if(m_undoEnabled)
			{
				if(!undo) undo = new MU_Select(SelectJoints);

				undo->setSelectionDifference(j,how,!how);
			}
		}

		m_joints[j]->m_selected = how;
	}

	if(ret&&!m_selecting) m_changeBits|=SelectionJoints; //2020

	sendUndo(undo); return ret;
}
bool Model::selectAllPoints(bool how)
{
	//https://github.com/zturtleman/mm3d/issues/90
	//if(m_undoEnabled) beginSelectionDifference(); //OVERKILL!

	bool ret = false;

	MU_Select *undo = nullptr; //2020

	for(unsigned p = 0; p<m_points.size(); p++)
	{
		if(how==m_points[p]->m_selected) continue;
		
		ret = true;

		if(!m_selecting) //2020: making fullproof?
		{
			if(m_undoEnabled)
			{
				if(!undo) undo = new MU_Select(SelectPoints);

				undo->setSelectionDifference(p,how,!how);
			}
		}

		m_points[p]->m_selected = how;
	}

	if(ret&&!m_selecting) m_changeBits|=SelectionPoints; //2020

	sendUndo(undo); return ret;
}
bool Model::selectAllProjections(bool how)
{
	//https://github.com/zturtleman/mm3d/issues/90
	//if(m_undoEnabled) beginSelectionDifference(); //OVERKILL!

	bool ret = false;

	MU_Select *undo = nullptr; //2020

	for(unsigned p = 0; p<m_projections.size(); p++)
	{
		if(how==m_projections[p]->m_selected) continue;
		
		ret = true;

		if(!m_selecting) //2020: making fullproof?
		{
			if(m_undoEnabled)
			{
				if(!undo) undo = new MU_Select(SelectProjections);

				undo->setSelectionDifference(p,how,!how);
			}
		}

		m_projections[p]->m_selected = how;
	}

	if(ret&&!m_selecting) m_changeBits|=SelectionProjections; //2020

	sendUndo(undo); return ret;
}
bool Model::selectAllPositions(PositionTypeE pt, bool how)
{
	switch(pt)
	{
	case PT_Vertex: return selectAllVertices(how); 
	case PT_Joint: return selectAllBoneJoints(how);
	case PT_Point: return selectAllPoints(how);
	case PT_Projection: return selectAllProjections(how);
	}
	return false;
}
void Model::selectAll(bool how)
{
	//LOG_PROFILE(); //???

	//Joints window calls unselectAllBoneJoints independent
	//of unslectAll so endSelectionDifference isn't called
	//beginSelectionDifference(); //OVERKILL!

	selectAllVertices(how);
	selectAllTriangles(how);
	selectAllGroups(how);
	selectAllBoneJoints(how);
	selectAllPoints(how);
	selectAllProjections(how);

	//endSelectionDifference();
	//return true;
}

void Model::selectFreeVertices()
{
	beginSelectionDifference(); //OVERKILL!

	/*
	//unsigned vcount = m_vertices.size();
	//unsigned v = 0;

	//WARNING! This isn't finding free vertices that aren't marked
	//with m_free!? MM3D may (or may not) make these but external
	//models can have them (i.e. exported MM3D files)

	for(auto*ea:m_vertices)
	{
		ea->m_marked = ea->m_free;
	}

	unsigned tcount = m_triangles.size();

	for(unsigned t = 0; t<tcount; t++)
	{
		m_vertices[m_triangles[t]->m_vertexIndices[0]]->m_marked = false;
		m_vertices[m_triangles[t]->m_vertexIndices[1]]->m_marked = false;
		m_vertices[m_triangles[t]->m_vertexIndices[2]]->m_marked = false;
	}
	*/
	for(auto*ea:m_vertices)
	{
		/*
		//2020: beginSelectionDifference doesn't work like this!
		//ea->m_marked?selectVertex(v):unselectVertex(v);
		ea->m_selected = ea->m_marked;*/
		if(ea->m_faces.empty()) ea->m_selected = true;
	}

	endSelectionDifference();
}

bool Model::setSelectedTriangles(const int_list &l) //2022
{
	assert(!m_selecting);

	auto undo = m_undoEnabled?new MU_Select(SelectTriangles):0;

	bool sel = false;

	int li = 0, next = l.empty()?-1:l.front();

	int i = -1; for(auto*ea:m_triangles)
	{
		if(next==++i)
		{
			next = ++li<(int)l.size()?l[li]:-1;

			if(!ea->m_selected&&ea->m_visible)
			{
				sel = true;
			
				ea->m_selected = true;
			}
			else continue;
		}
		else if(ea->m_selected)
		{
			sel = true;
			
			ea->m_selected = false;
		}
		else continue;

		//if(!m_selecting) //2020: making fullproof?
		{
			if(undo) //m_undoEnabled
			{
				//if(!undo) undo = new MU_Select(SelectTriangles);

				bool how = ea->m_selected;

				undo->setSelectionDifference(i,how,!how);
			}
		}
	}
		
	if(sel) //HACK: Emulate MU_Select?
	{	
		bool o = setUndoEnabled(false);
		{
			selectVerticesFromTriangles();
		}
		setUndoEnabled(o);

		m_changeBits |= SelectionFaces;
	}
	
	sendUndo(undo); return sel;
}

void Model::setSelectedUv(const int_list &uvList)
{
	//Inappropriate. Nothing sees UVs.
	//m_changeBits |= SelectionChange; //2019
	m_changeBits |= SelectionUv; //2020

	if(m_undoEnabled)
	{
		auto undo = new MU_SetSelectedUv;
		undo->setSelectedUv(uvList,m_selectedUv);

		//HACK: I can't recall if this is technically necessary
		//now, but it's very annoying to have to undo/redo this
		//separate from face selection.
		//sendUndo(undo);
		appendUndo(undo);
	}

	if(&uvList!=&m_selectedUv) //2020: optimization
	m_selectedUv = uvList;
}

void Model::getSelectedUv(std::vector<int> &uvList)const
{
	uvList = m_selectedUv;
}

void Model::clearSelectedUv()
{
	std::vector<int> empty; setSelectedUv(empty);
}

void Model::getSelectedInterpolation(unsigned anim, unsigned frame, Get3<Interpolate2020E> pred)
{
	auto ab = _anim(anim); if(!ab) return;

	//REMINDER: Vertices are filled in first!

	if(~ab->m_frame0) //ANIMMODE_FRAME
	{
		auto fp = ab->m_frame0+frame;
		Interpolate2020E e[3] = {*e,InterpolateVoid,InterpolateVoid};
		if(ab->_frame_count()) //OUCH
		for(auto*ea:m_vertices) if(ea->m_selected)
		{
			 pred(&(e[0]=ea->m_frames[fp]->m_interp2020));
		}
	}
	//if(am==ANIMMODE_SKELETAL)
	{
		Keyframe cmp;
		cmp.m_frame = frame;
		for(auto&ea:ab->m_keyframes)
		{
			auto o = getPositionObject(ea.first);
			assert(o);
			if(!o||!o->m_selected) continue;

			auto er = std::equal_range(ea.second.begin(),ea.second.end(),&cmp,
			[](Keyframe *a, Keyframe *b)->bool{ return a->m_frame<b->m_frame; });

			Interpolate2020E e[3] = {};
			while(er.first<er.second)
			{
				auto &kf = **er.first++; e[kf.m_isRotation>>1] = kf.m_interp2020;
			}

			pred(e);
		}
	}	
}

#endif // MM3D_EDIT
