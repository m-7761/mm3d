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

#include "glmath.h"

static const double EQ_TOLERANCE = 0.00001;

Matrix::Matrix()
{
	loadIdentity();
}

void Matrix::loadIdentity()
{
	memset(m_val,0,sizeof(m_val));
	m_val[0] = 1.0;
	m_val[5] = 1.0;
	m_val[10] = 1.0;
	m_val[15] = 1.0;
}

bool Matrix::isIdentity()const
{
	for(int c = 0; c<4; ++c)
	{
		for(int r = 0; r<4; ++r)
		{
			if(!float_equiv(get(r,c),(r==c)? 1.0 : 0.0))
				return false;
		}
	}
	return true;
}

void Matrix::show()const
{
	for(int r = 0; r<4; r++)
	{
		for(int c = 0; c<4; c++)
		{
			printf("%0.2f ",m_val[(r<<2)+c]);
		}
		printf("\n");
	}
}

bool Matrix::operator==(const Matrix &rhs)const
{
	for(int c = 0; c<4; ++c)
	{
		for(int r = 0; r<4; ++r)
		{
			if(!float_equiv(get(r,c),rhs.get(r,c)))
				return false;
		}
	}
	return true;
}

bool Matrix::equiv(const Matrix &rhs, double tolerance)const
{
	Vector lright(2,0,0);
	Vector lup(0,2,0);
	Vector lfront(0,0,2);

	Vector rright(2,0,0);
	Vector rup(0,2,0);
	Vector rfront(0,0,2);

	apply4(lright);
	apply4(lup);
	apply4(lfront);

	rhs.apply4(rright);
	rhs.apply4(rup);
	rhs.apply4(rfront);

	if((lright-rright).mag()>tolerance)
		return false;
	if((lup-rup).mag()>tolerance)
		return false;
	if((lfront-rfront).mag()>tolerance)
		return false;

	return true;
}

void Matrix::setTranslation(const double *vector)
{
	m_val[12] = vector[0];
	m_val[13] = vector[1];
	m_val[14] = vector[2];
}

void Matrix::setTranslation(const Vector &vector)
{
	m_val[12] = vector[0];
	m_val[13] = vector[1];
	m_val[14] = vector[2];
}

void Matrix::setTranslation(const double &x, const double &y, const double &z)
{
	m_val[12] = x;
	m_val[13] = y;
	m_val[14] = z;
}

// takes array of three angles in radians
void Matrix::setRotation(const double radians[3])
{
	if(radians)
	{
		//rpy? roll, pitch, yaw? seems wrong
		//labels if so?
		double cr = cos(radians[0]);
		double sr = sin(radians[0]);
		double cp = cos(radians[1]);
		double sp = sin(radians[1]);
		double cy = cos(radians[2]);
		double sy = sin(radians[2]);
		double srsp = sr *sp;
		double crsp = cr *sp;

		m_val[ 0] = cp*cy;
		m_val[ 1] = cp*sy;
		m_val[ 2] = -sp;

		m_val[ 4] = srsp*cy-cr*sy;
		m_val[ 5] = srsp*sy+cr*cy;
		m_val[ 6] = sr*cp;

		m_val[ 8] = crsp*cy+sr*sy;
		m_val[ 9] = crsp*sy-sr*cy;
		m_val[10] = cr*cp;
	}
}

void Matrix::setRotation(const Vector &radians)
{
	setRotation(radians.getVector());
}

void Matrix::getRotation(double &x, double &y, double &z)const
{
	double sinYaw;
	double cosYaw;
	double sinPitch = -m_val[2];
	double cosPitch;
	double sinRoll;
	double cosRoll;

	// if sinPitch is close to 1.0 or -1.0 it's a gimbal lock
	if(fabs(sinPitch)+EQ_TOLERANCE>1.0)
	{
		cosPitch = 0.0;

		sinRoll = -m_val[9];
		cosRoll = m_val[5];
		sinYaw  = 0;
		cosYaw  = 1;
	}
	else
	{
		cosPitch = sqrt(1-sinPitch*sinPitch);

		sinRoll = m_val[6] /cosPitch;
		cosRoll = m_val[10]/cosPitch;
		sinYaw  = m_val[1] /cosPitch;
		cosYaw  = m_val[0] /cosPitch;
	}
	
	//rpy? roll, pitch, yaw? seems wrong
	//labels if so?
	z = atan2(sinYaw,cosYaw);
	y = atan2(sinPitch,cosPitch);
	x = atan2(sinRoll,cosRoll);
}

void Matrix::getRotation(double *radians)const
{
	if(radians) getRotation(radians[0],radians[1],radians[2]);
}

void Matrix::getRotation(Vector &radians)const
{
	getRotation(radians.getVector());
}

void Matrix::getTranslation(double &x, double &y, double &z)const
{
	x = m_val[12];
	y = m_val[13];
	z = m_val[14];
}

void Matrix::getTranslation(double *vector)const
{
	if(vector)
	{
		vector[0] = m_val[12];
		vector[1] = m_val[13];
		vector[2] = m_val[14];
	}
}

void Matrix::getTranslation(Vector &vector)const
{
	vector[0] = m_val[12];
	vector[1] = m_val[13];
	vector[2] = m_val[14];
}

void Matrix::setRotationInDegrees(const double *degrees)
{
	double radVector[3];
	radVector[0] = degrees[0] *PIOVER180;
	radVector[1] = degrees[1] *PIOVER180;
	radVector[2] = degrees[2] *PIOVER180;
	setRotation(radVector);
}

void Matrix::setRotationInDegrees(const Vector &degrees)
{
	setRotationInDegrees(degrees.getVector());
}

void Matrix::setRotationInDegrees(double x, double y, double z)
{
	double radVector[3];
	radVector[0] = x *PIOVER180;
	radVector[1] = y *PIOVER180;
	radVector[2] = z *PIOVER180;
	setRotation(radVector);
}

void Matrix::setInverseRotation(const double *radians)
{
	if(radians)
	{
		double cr = cos(-radians[0]);
		double sr = sin(-radians[0]);
		double cp = cos(-radians[1]);
		double sp = sin(-radians[1]);
		double cy = cos(-radians[2]);
		double sy = sin(-radians[2]);
		double srsp = sr *sp;
		double crsp = cr *sp;

		m_val[ 0] = cp *cy;
		m_val[ 1] = cp *sy;
		m_val[ 2] = -sp;

		m_val[ 4] = (srsp *cy)-(cr *sy);
		m_val[ 5] = (srsp *sy)+(cr *cy);
		m_val[ 6] = (sr *cp);

		m_val[ 8] = (crsp *cy)+(sr *sy);
		m_val[ 9] = (crsp *sy)-(sr *cy);
		m_val[10] = (cr *cp);
	}
}

void Matrix::setInverseRotationInDegrees(const double *degrees)
{
	double radVector[3];
	radVector[0] = degrees[0] *PIOVER180;
	radVector[1] = degrees[1] *PIOVER180;
	radVector[2] = degrees[2] *PIOVER180;
	setInverseRotation(radVector);
}

void Matrix::setRotationOnAxis(const double *pVect, double radians)
{
	if(pVect)
	{
		Quaternion quat;
		quat.setRotationOnAxis(pVect,radians);
		setRotationQuaternion(quat);
	}
}

void Matrix::setRotationQuaternion(const Quaternion &quat)
{
	auto q_val = quat.getVector();

	double xx = q_val[0]*q_val[0];
	double xy = q_val[0]*q_val[1];
	double xz = q_val[0]*q_val[2];
	double xw = q_val[0]*q_val[3];

	double yy = q_val[1]*q_val[1];
	double yz = q_val[1]*q_val[2];
	double yw = q_val[1]*q_val[3];

	double zz = q_val[2]*q_val[2];
	double zw = q_val[2]*q_val[3];

	m_val[0]  = 1-2 *(yy+zz);
	m_val[4]  =	  2 *(xy-zw);
	m_val[8]  =	  2 *(xz+yw);

	m_val[1]  =	  2 *(xy+zw);
	m_val[5]  = 1-2 *(xx+zz);
	m_val[9]  =	  2 *(yz-xw);

	m_val[2]  =	  2 *(xz-yw);
	m_val[6]  =	  2 *(yz+xw);
	m_val[10] = 1-2 *(xx+yy);

	m_val[3]  = m_val[7] = m_val[11] = m_val[12] = m_val[13] = m_val[14] = 0;
	m_val[15] = 1;
}

void Matrix::getRotationQuaternion(Quaternion &quat)const
{
	double s;

	double t = 1+m_val[0]+m_val[5]+m_val[10];
	if(t>0.00001)
	{
		s = sqrt(t)*2;
		quat.set(0,(m_val[6]-m_val[9])/s);
		quat.set(1,(m_val[8]-m_val[2])/s);
		quat.set(2,(m_val[1]-m_val[4])/s);
		quat.set(3,0.25 *s);
	}
	else
	{
		if(m_val[0]>m_val[5]&&m_val[0]>m_val[10])// Column 0
		{
			s = sqrt(1.0+m_val[0]-m_val[5]-m_val[10])*2;
			quat.set(0,0.25 *s);
			quat.set(1,(m_val[1]+m_val[4])/s);
			quat.set(2,(m_val[8]+m_val[2])/s);
			quat.set(3,(m_val[6]-m_val[9])/s);
		}
		else if(m_val[5]>m_val[10])// Column 1
		{
			s = sqrt(1.0+m_val[5]-m_val[0]-m_val[10])*2;
			quat.set(0,(m_val[1]+m_val[4])/s);
			quat.set(1,0.25 *s);
			quat.set(2,(m_val[6]+m_val[9])/s);
			quat.set(3,(m_val[8]-m_val[2])/s);
		}
		else // Column 2
		{
			s = sqrt(1.0+m_val[10]-m_val[0]-m_val[5])*2;
			quat.set(0,(m_val[8]+m_val[2])/s);
			quat.set(1,(m_val[6]+m_val[9])/s);
			quat.set(2,0.25 *s);
			quat.set(3,(m_val[1]-m_val[4])/s);
		}
	}
}

void Matrix::inverseRotateVector(double *pVect)const
{
	 double vec[3];

	 vec[0] = pVect[0] *m_val[0]+pVect[1] *m_val[1]+pVect[2] *m_val[2];
	 vec[1] = pVect[0] *m_val[4]+pVect[1] *m_val[5]+pVect[2] *m_val[6];
	 vec[2] = pVect[0] *m_val[8]+pVect[1] *m_val[9]+pVect[2] *m_val[10];

	 memcpy(pVect,vec,sizeof(double)*3);
}
void Matrix::rotateVector(double *pVect)const //2020
{
	 double vec[3];

	 vec[0] = pVect[0] *m_val[0]+pVect[1] *m_val[4]+pVect[2] *m_val[8];
	 vec[1] = pVect[0] *m_val[1]+pVect[1] *m_val[5]+pVect[2] *m_val[9];
	 vec[2] = pVect[0] *m_val[2]+pVect[1] *m_val[6]+pVect[2] *m_val[10];

	 memcpy(pVect,vec,sizeof(double)*3);
}

void Matrix::inverseTranslateVector(double *pVect)const
{
	 pVect[0] = pVect[0]-m_val[12];
	 pVect[1] = pVect[1]-m_val[13];
	 pVect[2] = pVect[2]-m_val[14];
}
void Matrix::translateVector(double *pVect)const //2020
{
	 pVect[0] = pVect[0]+m_val[12];
	 pVect[1] = pVect[1]+m_val[13];
	 pVect[2] = pVect[2]+m_val[14];
}

void Matrix::normalizeRotation()
{
	normalize3(&m_val[0]);
	normalize3(&m_val[4]);
	normalize3(&m_val[8]);
}
void Matrix::getScale(double xyz[3])const //2020
{
	xyz[0] = mag3(&m_val[0]);
	xyz[1] = mag3(&m_val[4]);
	xyz[2] = mag3(&m_val[8]);
}

void Matrix::apply4(float *pVec)const
{
	if(pVec)
	{
		double vec[4];
		for(int c = 0; c<4; c++)
		{
			vec[c] 
				= pVec[0] *m_val[(0<<2)+c]
			  +pVec[1] *m_val[(1<<2)+c]
			  +pVec[2] *m_val[(2<<2)+c]
			  +pVec[3] *m_val[(3<<2)+c] ;
		}

		pVec[0] = (float)vec[0];
		pVec[1] = (float)vec[1];
		pVec[2] = (float)vec[2];
		pVec[3] = (float)vec[3];
	}
}

void Matrix::apply4(double *pVec)const
{
	if(pVec)
	{
		double vec[4];
		for(int c = 0; c<4; c++)
		{
			vec[c] 
				= pVec[0] *m_val[(0<<2)+c]
			  +pVec[1] *m_val[(1<<2)+c]
			  +pVec[2] *m_val[(2<<2)+c]
			  +pVec[3] *m_val[(3<<2)+c] ;
		}

		pVec[0] = vec[0];
		pVec[1] = vec[1];
		pVec[2] = vec[2];
		pVec[3] = vec[3];
	}
}

void Matrix::apply4(Vector &pVec)const
{
	apply4(pVec.getVector());
}

void Matrix::apply3(float *pVec)const
{
	if(pVec)
	{
		double vec[4];
		for(int c = 0; c<3; c++)
		{
			vec[c] 
				= pVec[0] *m_val[(0<<2)+c]
			  +pVec[1] *m_val[(1<<2)+c]
			  +pVec[2] *m_val[(2<<2)+c];
		}

		pVec[0] = (float)vec[0];
		pVec[1] = (float)vec[1];
		pVec[2] = (float)vec[2];
	}
}

void Matrix::apply3(double *pVec)const
{
	if(pVec)
	{
		double vec[3];
		for(int c = 0; c<3; c++)
		{
			vec[c] 
				= pVec[0] *m_val[(0<<2)+c]
			  +pVec[1] *m_val[(1<<2)+c]
			  +pVec[2] *m_val[(2<<2)+c];
		}

		pVec[0] = vec[0];
		pVec[1] = vec[1];
		pVec[2] = vec[2];
	}
}

void Matrix::apply3(Vector &pVec)const
{
	apply3(pVec.getVector());
}

void Matrix::apply3x(float *pVec)const
{
	if(pVec)
	{
		double vec[4];
		for(int c = 0; c<3; c++)
		{
			vec[c] 
				= pVec[0] *m_val[(0<<2)+c]
			  +pVec[1] *m_val[(1<<2)+c]
			  +pVec[2] *m_val[(2<<2)+c]
			  +/*+1.0f* */m_val[(3<<2)+c];
		}

		pVec[0] = (float)vec[0];
		pVec[1] = (float)vec[1];
		pVec[2] = (float)vec[2];
	}
}

void Matrix::apply3x(double *pVec)const
{
	if(pVec)
	{
		double vec[3];
		for(int c = 0; c<3; c++)
		{
			vec[c] 
				= pVec[0] *m_val[(0<<2)+c]
			  +pVec[1] *m_val[(1<<2)+c]
			  +pVec[2] *m_val[(2<<2)+c]
			  +/*1.0* */m_val[(3<<2)+c];
		}

		pVec[0] = vec[0];
		pVec[1] = vec[1];
		pVec[2] = vec[2];
	}
}

void Matrix::apply3x(Vector &pVec)const
{
	apply3x(pVec.getVector());
}

void Matrix::postMultiply(const Matrix &lhs)
{
	double val[16];
	int r;
	int c;

	for(r = 0; r<4; r++)
	{
		for(c = 0; c<4; c++)
		{
			val[(r<<2)+c] 
				= lhs.m_val[(r<<2)+0] *m_val[(0<<2)+c]
			  +lhs.m_val[(r<<2)+1] *m_val[(1<<2)+c]
			  +lhs.m_val[(r<<2)+2] *m_val[(2<<2)+c]
			  +lhs.m_val[(r<<2)+3] *m_val[(3<<2)+c]
				;
		}
	}

	for(r = 0; r<16; r++)
	{
		m_val[r] = val[r];
	}
}

double Matrix::getDeterminant3()const
{
	double det = 
		  m_val[0] *(m_val[5]*m_val[10]-m_val[6]*m_val[9])
	  -m_val[4] *(m_val[1]*m_val[10]-m_val[2]*m_val[9])
	  +m_val[8] *(m_val[1]*m_val[6] -m_val[2]*m_val[5]);

	return det;
}

double Matrix::getDeterminant()const
{
	double result = 0;

	Matrix msub3;
	for(int n = 0,i = 1; n<4; n++,i *= -1)
	{
		getSubMatrix(msub3,0,n);

		double det = msub3.getDeterminant3();
		result += m_val[n] *det *i;
	}

	return(result);
}

void Matrix::getSubMatrix(Matrix &ret, int i, int j)const
{
	for(int di = 0; di<3; di ++)
	{
		for(int dj = 0; dj<3; dj ++)
		{
			int si = di+((di>=i)? 1 : 0);
			int sj = dj+((dj>=j)? 1 : 0);

			ret.set(di,dj,m_val[(si<<2)+sj]);
		}
	}
}

Matrix Matrix::getInverse()const
{
	Matrix mr;

	double mdet = getDeterminant();

	if(fabs(mdet)<0.0005)
	{
		return mr;
	}

	Matrix mtemp;
	for(int i = 0; i<4; i++)
	{
		for(int j = 0; j<4; j++)
		{
			int sign = 1-((i +j)%2)*2;
			getSubMatrix(mtemp,i,j);
			mr.set(j,i,(mtemp.getDeterminant3()*sign)/mdet);
		}
	}

	return mr;
}


Matrix operator*(const Matrix &lhs, const Matrix &rhs)
{
	Matrix m;
	for(int r = 0; r<4; r++)
	{
		for(int c = 0; c<4; c++)
		{
			m.m_val[(r<<2)+c] 
				= lhs.m_val[(r<<2)+0] *rhs.m_val[(0<<2)+c]
			  +lhs.m_val[(r<<2)+1] *rhs.m_val[(1<<2)+c]
			  +lhs.m_val[(r<<2)+2] *rhs.m_val[(2<<2)+c]
			  +lhs.m_val[(r<<2)+3] *rhs.m_val[(3<<2)+c]
				;
		}
	}

	return m;
}

Vector operator*(const Vector &lhs, const Matrix &rhs)
{
	Vector m;
	for(int r = 0; r<1; r++)
	{
		for(int c = 0; c<4; c++)
		{
			m.m_val[(r<<2)+c] 
				= lhs.m_val[(r<<2)+0] *rhs.m_val[(0<<2)+c]
			  +lhs.m_val[(r<<2)+1] *rhs.m_val[(1<<2)+c]
			  +lhs.m_val[(r<<2)+2] *rhs.m_val[(2<<2)+c]
			  +lhs.m_val[(r<<2)+3] *rhs.m_val[(3<<2)+c]
				;
		}
	}

	return m;
}

Vector operator*(const Vector &lhs, const double &rhs)
{
	Vector v;
	v.m_val[0] = lhs.m_val[0] *rhs;
	v.m_val[1] = lhs.m_val[1] *rhs;
	v.m_val[2] = lhs.m_val[2] *rhs;
	v.m_val[3] = lhs.m_val[3] *rhs;
	return v;
}
Vector operator*(const double &rhs, const Vector &lhs)
{
	Vector v;
	v.m_val[0] = lhs.m_val[0] *rhs;
	v.m_val[1] = lhs.m_val[1] *rhs;
	v.m_val[2] = lhs.m_val[2] *rhs;
	v.m_val[3] = lhs.m_val[3] *rhs;
	return v;
}

Vector operator-(const Vector &lhs, const Vector &rhs)
{
	Vector v;
	v.m_val[0] = lhs.m_val[0]-rhs.m_val[0];
	v.m_val[1] = lhs.m_val[1]-rhs.m_val[1];
	v.m_val[2] = lhs.m_val[2]-rhs.m_val[2];
	v.m_val[3] = lhs.m_val[3]-rhs.m_val[3];
	return v;
}

Vector operator+(const Vector &lhs, const Vector &rhs)
{
	Vector v;
	v.m_val[0] = lhs.m_val[0]+rhs.m_val[0];
	v.m_val[1] = lhs.m_val[1]+rhs.m_val[1];
	v.m_val[2] = lhs.m_val[2]+rhs.m_val[2];
	v.m_val[3] = lhs.m_val[3]+rhs.m_val[3];
	return v;
}

Quaternion operator*(const Quaternion &lhs, const Quaternion &rhs)
{
	Quaternion res;

	res.m_val[0] = lhs.m_val[3]*rhs.m_val[0]+lhs.m_val[0]*rhs.m_val[3]+lhs.m_val[1]*rhs.m_val[2]-lhs.m_val[2]*rhs.m_val[1];
	res.m_val[1] = lhs.m_val[3]*rhs.m_val[1]+lhs.m_val[1]*rhs.m_val[3]+lhs.m_val[2]*rhs.m_val[0]-lhs.m_val[0]*rhs.m_val[2];
	res.m_val[2] = lhs.m_val[3]*rhs.m_val[2]+lhs.m_val[2]*rhs.m_val[3]+lhs.m_val[0]*rhs.m_val[1]-lhs.m_val[1]*rhs.m_val[0];
	res.m_val[3] = lhs.m_val[3]*rhs.m_val[3]-lhs.m_val[0]*rhs.m_val[0]-lhs.m_val[1]*rhs.m_val[1]-lhs.m_val[2]*rhs.m_val[2];

	return res;
}
Vector operator*(const Vector &lhs, const Quaternion &rhs) //q*v*q-1
{
	Quaternion q(lhs,0),conj(rhs.neg3(),rhs.m_val[3]); //or (v,-s) ?

	q = rhs * q * conj; q.m_val[3] = lhs[3]; return q;
}

Vector::Vector(const double *val)
{
	if(val)
	{
		m_val[0] = val[0];
		m_val[1] = val[1];
		m_val[2] = val[2];
		m_val[3] = 1.0;
	}
	else
	{
		m_val[0] = 0.0;
		m_val[1] = 0.0;
		m_val[2] = 0.0;
		m_val[3] = 1.0;
	}
}

Vector::Vector(const double &x, const double &y, const double &z, const double &w)
{
	m_val[0] = x;
	m_val[1] = y;
	m_val[2] = z;
	m_val[3] = w;
}

void Vector::set(int c, double val)
{
	m_val[c] = val;
}

void Vector::setAll(double x, double y, double z, double w)
{
	m_val[0] = x;
	m_val[1] = y;
	m_val[2] = z;
	m_val[3] = w;
}

void Vector::setAll(const double *vec, int count)
{
	for(int i = 0; i<count&&i<4; i++)
	{
		m_val[i] = vec[i];
	}
}

void Vector::show()const
{
	for(int c = 0; c<4; c++)
	{
		printf("%.2f ",m_val[c]);
	}
	printf("\n");
}

/*//??? //REMOVE ME
void Vector::translate(const Matrix &rhs
{
	Vector v;
	for(int r = 0; r<4; r++)
	{
		v.m_val[r] 
		 = m_val[0] *rhs.m_val[(0<<2)+r]
		  +m_val[1] *rhs.m_val[(1<<2)+r]
		  +m_val[2] *rhs.m_val[(2<<2)+r]
		  +m_val[3] *rhs.m_val[(3<<2)+r]
			;
	}

	*this = v;
}*/
void Vector::transform(const Matrix& rhs)
{
	 double vector[4];
	 const double *matrix = rhs.getMatrix();

	 //2020: Shouldn't m_val[3] be included in this calculation?
	 vector[0] = m_val[0]*matrix[0] +m_val[1]*matrix[4] +m_val[2]*matrix[8] /*m_val[3]*/+matrix[12];
	 vector[1] = m_val[0]*matrix[1] +m_val[1]*matrix[5] +m_val[2]*matrix[9] /*m_val[3]*/+matrix[13];
	 vector[2] = m_val[0]*matrix[2] +m_val[1]*matrix[6] +m_val[2]*matrix[10] /*m_val[3]*/+matrix[14];
	 vector[3] = m_val[0]*matrix[3] +m_val[1]*matrix[7] +m_val[2]*matrix[11] /*m_val[3]*/+matrix[15];

	 m_val[0] = (double)(vector[0]);
	 m_val[1] = (double)(vector[1]);
	 m_val[2] = (double)(vector[2]);
	 m_val[3] = (double)(vector[3]);
}

void Vector::transform3(const Matrix& rhs)
{
	 double vector[4];
	 const double *matrix = rhs.getMatrix();

	 vector[0] = m_val[0] *matrix[0]+m_val[1] *matrix[4]+m_val[2] *matrix[8];
	 vector[1] = m_val[0] *matrix[1]+m_val[1] *matrix[5]+m_val[2] *matrix[9];
	 vector[2] = m_val[0] *matrix[2]+m_val[1] *matrix[6]+m_val[2] *matrix[10];

	 m_val[0] = (double)(vector[0]);
	 m_val[1] = (double)(vector[1]);
	 m_val[2] = (double)(vector[2]);
	 m_val[3] = 1;
}

void Vector::scale(double scale)
{
	for(int t = 0; t<4; t++)
	{
		m_val[t] *= scale;
	}
}

void Vector::scale3(double scale)
{
	for(int t = 0; t<3; t++)
	{
		m_val[t] *= scale;
	}
}

double Vector::mag()const
{
	return sqrt(  m_val[0]*m_val[0]
					+m_val[1]*m_val[1]
					+m_val[2]*m_val[2]
					+m_val[3]*m_val[3]);
}

double Vector::mag3()const
{
	return sqrt(dot3(*this));
}

double Vector::normalize()
{
	double r_val = mag();
	if(double m=r_val) //2022: Zero divide?
	{
		m = 1/m;
		for(int t=4;t-->0;) m_val[t]*=m;
	}
	return r_val;
}

double Vector::normalize3()
{
	double r_val = mag3();
	if(double m=r_val) //2022: Zero divide?
	{
		m = 1/m;
		for(int t=3;t-->0;) m_val[t]*=m; 
	}
	return r_val;
}

double Vector::dot3(const Vector &rhs)const
{
	return(  m_val[0] *rhs.m_val[0]
			 +m_val[1] *rhs.m_val[1]
			 +m_val[2] *rhs.m_val[2]);
}

double Vector::dot4(const Vector &rhs)const
{
	return(  m_val[0] *rhs.m_val[0]
			 +m_val[1] *rhs.m_val[1]
			 +m_val[2] *rhs.m_val[2]
			 +m_val[3] *rhs.m_val[3]);
}

Vector Vector::cross3(const Vector &rhs)const
{
	Vector rval;

	rval.m_val[0] = m_val[1]*rhs.m_val[2]-rhs.m_val[1]*m_val[2];
	rval.m_val[1] = m_val[2]*rhs.m_val[0]-rhs.m_val[2]*m_val[0];
	rval.m_val[2] = m_val[0]*rhs.m_val[1]-rhs.m_val[0]*m_val[1];

	return rval;
}

double &Vector::operator[](int index)
{
	return m_val[index];
}

const double &Vector::operator[](int index)const
{
	return m_val[index];
}

Vector &Vector::operator+=(const Vector &rhs)
{
	this->m_val[0] += rhs.m_val[0];
	this->m_val[1] += rhs.m_val[1];
	this->m_val[2] += rhs.m_val[2];
	this->m_val[3] += rhs.m_val[3];
	return *this;
}

Vector &Vector::operator-=(const Vector &rhs)
{
	this->m_val[0] -= rhs.m_val[0];
	this->m_val[1] -= rhs.m_val[1];
	this->m_val[2] -= rhs.m_val[2];
	this->m_val[3] -= rhs.m_val[3];
	return *this;
}

bool Vector::operator==(const Vector &rhs)const
{
	return floatCompareVector(this->getVector(),rhs.getVector(),4);
}

void Quaternion::show()const
{
	for(int c = 0; c<4; c++)
	{
		printf("%.2f ",m_val[c]);
	}
	printf("\n");
}

void Quaternion::setEulerAngles(const double *radians)
{
	double axis[4];

	Quaternion x;
	axis[0] = 1.0;
	axis[1] = 0.0;
	axis[2] = 0.0;
	x.setRotationOnAxis(axis,radians[0]);

	Quaternion y;
	axis[0] = 0.0;
	axis[1] = 1.0;
	axis[2] = 0.0;
	y.setRotationOnAxis(axis,radians[1]);

	Quaternion z;
	axis[0] = 0.0;
	axis[1] = 0.0;
	axis[2] = 1.0;
	z.setRotationOnAxis(axis,radians[2]);

	*this = z * y * x; //post-multiply?
}

void Quaternion::setRotationOnAxis(const double *axis, double radians)
{
	//if(axis&&::mag3(axis)>0.00001) //???
	if(axis&&::squared_mag3(axis)>0.00001) //???
	{
		for(int t=0;t<3;t++)		
		m_val[t] = axis[t];
		m_val[3] = 0;
		normalize3();

		double a = sin(radians/2);
		double b = cos(radians/2);

		m_val[0] *= a;
		m_val[1] *= a;
		m_val[2] *= a;
		m_val[3]  = b;
	}
}

void Quaternion::setRotationOnAxis(double x, double y, double z, double radians)
{
	double m = /*sqrt*/(x*x+y*y+z*z); //???
	if(m>0.00001)
	{
		m_val[0] = x;
		m_val[1] = y;
		m_val[2] = z;
		m_val[3] = 0;

		normalize3();

		double a = sin(radians/2);
		double b = cos(radians/2);

		m_val[0] *= a;
		m_val[1] *= a;
		m_val[2] *= a;
		m_val[3]  = b;
	}
}

/*
void Quaternion::setRotationToPoint(const Vector &face, const Vector &point)
{	
	//???
	//Vector v1(faceX,faceY,faceZ); // Facing
	//Vector v2(pointX,pointY,pointZ); // Want to face
	Vector v1 = face; v1.normalize3();
	Vector v2 = point; v2.normalize3();

	//This fails if the angle is Pi (180) but I'm not sure 
	//what is the best fix. The normal will be degenerated.
	double anglec = v1.dot3(v2);
	Vector normal;
	if(anglec>-1&&anglec<1) //2020
	{
		normal = v1.cross3(v2);
		normal.normalize3();
	}
	else normal.setAll(v2.get(1),v2.get(0),v2.get(3)); //HACK

	setRotationOnAxis(normal.getVector(),acos(anglec));
}*/

void Quaternion::getRotationOnAxis(double *axis, double &radians)const
{
	Quaternion q(m_val);
	q.normalize();

	double cos_a = q.m_val[3];
	radians = acos(cos_a)*2;

	double sin_a = sqrt(1.0-cos_a *cos_a);
	if(fabs(sin_a)<0.0005)
	{
		sin_a = 1.0;
	}

	axis[0] = q.m_val[0]/sin_a;
	axis[1] = q.m_val[1]/sin_a;
	axis[2] = q.m_val[2]/sin_a;
}

template<int H, int A, int B, int C>
///////////////////////////////////////////////////////////////////////////
//   template arguments -
// H: handedness. possible values, +1 for right handed, -1 for left handed.
// A: first angle in a Euler angle sequence; 1 for X, 2 for Y, 3 for Z.
// B: second angle; C, and so on. 
///////////////////////////////////////////////////////////////////////////
static void glmath_EulerAnglesFromQuaternion(double vOut[3], const Quaternion &qIn)
{
	//! assuming normalized
	Quaternion qOut(qIn[A],qIn[B],qIn[C],qIn[3]);	

	double test = qOut[3]*qOut[1] + qOut[0]*qOut[2]*float(H);

	//TESTING: these values are producing turbulence (86.3 degrees?)
	//https://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/jack.htm
	//if(test>0.499f) //! singularity at north pole
	if(test>=0.499999)
	{
		//2020: I'm adding H since I observed the sign is incorrect.
		vOut[0] = atan2(qOut[0],qOut[3])*+2*H;
		vOut[1] = PI/2;
		vOut[2] = 0;

		return;
	}
	//if(test<-0.499f) //! singularity at south pole
	if(test<=-0.499999)
	{
		//2020: I'm adding H since I observed the sign is incorrect.
		vOut[0] = atan2(qOut[0],qOut[3])*-2*H;
		vOut[1] = -PI/2;
		vOut[2] = 0;

		return;
	}

	double sqx = qOut[0]*qOut[0];
	double sqy = qOut[1]*qOut[1];
	double sqz = qOut[2]*qOut[2];

	vOut[0] = atan2(2*qOut[3]*qOut[0]-2*qOut[1]*qOut[2]*H,1-2*sqx-2*sqy);
	vOut[1] = asin(2*test);
	vOut[2] = atan2(2*qOut[3]*qOut[2]-2*qOut[0]*qOut[1]*H,1-2*sqy-2*sqz);
} 
void Quaternion::getEulerAngles(double radians[3])
{
	glmath_EulerAnglesFromQuaternion<-1,0,1,2>(radians,*this);
}

Quaternion &Quaternion::normalize()
{
	double mag = 0;
	for(int t=4;t-->0;)
	mag+=m_val[t]*m_val[t];

	//2022: Vector::normalize needs a
	//zero divide test... I don't know
	//how common 0 is for quaternions
	//but in theory it's a possibility.
	if(double m=mag)
	{
		m = 1/sqrt(m);

		for(int t=4;t-->0;) m_val[t]*=m;
	}
	
	return *this;
}

/*REMOVE ME
Quaternion Quaternion::swapHandedness()
{
	double axis[3] = { 0,0,0 };
	double rad = 0;

	getRotationOnAxis(axis,rad);

	Quaternion rval;
	rval.setRotationOnAxis(axis,-rad);

	return rval;
}*/

double distance(const Vector &v1, const Vector &v2)
{
	double xDiff = v2.get(0)-v1.get(0);
	double yDiff = v2.get(1)-v1.get(1);
	double zDiff = v2.get(2)-v1.get(2);

	return sqrt(xDiff*xDiff+yDiff*yDiff+zDiff*zDiff);
}

double distance(const double v1[3], const double v2[3])
{
	double xDiff = v2[0]-v1[0];
	double yDiff = v2[1]-v1[1];
	double zDiff = v2[2]-v1[2];

	return sqrt(xDiff*xDiff+yDiff*yDiff+zDiff*zDiff);
}

