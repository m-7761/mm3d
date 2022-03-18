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


#ifndef __GLMATH_H
#define __GLMATH_H

#include "mm3dtypes.h"

class Matrix
{
	public:
		Matrix();

		void loadIdentity();
		bool isIdentity()const;

		void show()const;
		void set(int r, int c, double val){ m_val[(r<<2)+c] = val; }
		double get(int r, int c)const { return m_val[(r<<2)+c]; }

		bool operator==(const Matrix &rhs)const;
		bool equiv(const Matrix &rhs, double tolerance = 0.00001)const;

		void scale(const double xyz[3]) //2020
		{
			for(int i=0;i<3;i++) 
			for(int j=0;j<3;j++) getVector(i)[j]*=xyz[i];
		}
		void getScale(double xyz[3])const; //2020

		void setTranslation(const Vector &vector);
		void setTranslation(const double *vector);
		void setTranslation(const double &x, const double &y, const double &z);

		void setRotation(const Vector &radians);
		void setRotation(const double *radians);
		void setRotationInDegrees(const double *degrees);
		void setRotationInDegrees(const Vector &degrees);
		void setRotationInDegrees(double x, double y, double z);
		void getRotation(double &x, double &y, double &z)const;
		void getRotation(Vector &radians)const;
		void getRotation(double *radians)const;
		void getTranslation(double &x, double &y, double &z)const;
		void getTranslation(Vector &vector)const;
		void getTranslation(double *vector)const;

		void setInverseRotation(const double *radians);
		void setInverseRotationInDegrees(const double *radians);

		void setRotationOnAxis(const double *pVect, double radians);
		void setRotationQuaternion(const Quaternion &quat);
		void getRotationQuaternion(Quaternion &quat)const;

		//NOTE: inverseRotateVector applies scaling to the rotation 
		void inverseTranslateVector(double *pVect)const;
		void inverseRotateVector(double *pVect)const;

		void translateVector(double *pVect)const; //2020
		void rotateVector(double *pVect)const; //2020

		void normalizeRotation();

		void apply(float *pVec)const;
		void apply(double *pVec)const;
		void apply(Vector &pVec)const;
		void apply3(float *pVec)const;
		void apply3(double *pVec)const;
		void apply3(Vector &pVec)const;
		void apply3x(float *pVec)const;  // Apply 4x4 matrix to 3 element vec
		void apply3x(double *pVec)const;
		void apply3x(Vector &pVec)const;

		//2019: This is equivalent to doing
		//glLoadMatrix(this);
		//glMultMatrix(lhs);
		void postMultiply(const Matrix &lhs);

		double getDeterminant()const;
		double getDeterminant3()const;
		Matrix getInverse()const;
		void getSubMatrix(Matrix &ret, int i, int j)const;

		double *getMatrix(){ return m_val; };
		const double *getMatrix()const{ return m_val; };

		friend Matrix operator*(const Matrix &lhs, const Matrix &rhs);
		friend Vector operator*(const Vector &lhs, const Matrix &rhs);
		friend class Vector;

		const double *getVector(int r)const{ return m_val+r*4; };
		double *getVector(int r){ return m_val+r*4; };

	protected:

		//const double *getMatrix()const{ return m_val; }; //???

		double m_val[16];
};

class Vector
{
	public:
		Vector(const double *val = nullptr);
		Vector(const double &x, const double &y, const double &z, const double &w = 1.0);
		Vector(const Vector &val, double w)
		{
			for(int i=3;i-->0;)
			m_val[i] = val.m_val[i]; m_val[3] = w;
		}

		void show()const;

		//NOTE: This does/did 4 component transform
		//REMOVE ME
		//Is this used?
		//What is translation by matrix supposed to mean?
		//void translate(const Matrix &rhs);

		//WARNING: transform ASSUMES w IS 1
		void transform(const Matrix &rhs);
		void transform3(const Matrix &rhs);
		void set(int c, double val);
		void setAll(double x, double y, double z, double w = 1.0);
		void setAll(const double *vec, int count = 3);
		double get(int c)const { return m_val[c]; };

		void scale(double val);
		void scale3(double val);

		double mag()const;
		double mag3()const;

		double normalize();
		double normalize3();

		double dot3(const Vector &rhs)const;
		double dot4(const Vector &rhs)const;
		Vector cross3(const Vector &rhs)const;

		const double *getVector()const { return m_val; };
		double *getVector(){ return m_val; };

		void getVector(double(&v3)[3])const //2020
		{
			for(int i=3;i-->0;) v3[i] = m_val[i];
		}
		void getVector3(double v3[3])const //2020
		{
			for(int i=3;i-->0;) v3[i] = m_val[i];
		}

		double &operator[](int index);
		const double &operator[](int index)const;
		Vector &operator+=(const Vector &rhs);
		Vector &operator-=(const Vector &rhs);
		bool operator==(const Vector &rhs)const;

		friend Vector operator*(const Vector &lhs, const Matrix &rhs);
		friend Vector operator*(const Vector &lhs, const double &rhs);
		friend Vector operator*(const double &rhs, const Vector &lhs);
		friend Vector operator-(const Vector &lhs, const Vector &rhs);
		friend Vector operator+(const Vector &lhs, const Vector &rhs);

		//Vector operator-()const //2020
		Vector neg3()const //2020
		{
			return Vector(-m_val[0],-m_val[1],-m_val[2],/*-*/m_val[3]);
		}

	protected:
		double m_val[4];
};

class Quaternion : public Vector
{
	public:

		using Vector::Vector; //C++11		
		Quaternion(const Vector &cp):Vector(cp) //C++
		{} 
		Quaternion(const double *val=nullptr):Vector(val)
		{
			if(val) m_val[3] = val[3]; //differs?
		}

		void show()const;
		void setEulerAngles(const double *radians);
		void getEulerAngles(double radians[3]); //2020
		void setRotationOnAxis(double x, double y, double z, double radians);
		void setRotationOnAxis(const double *axis, double radians);
		void getRotationOnAxis(double *axis, double &radians)const;		
		void set(int c, double val);
		double get(int c)const { return m_val[c]; };

		Quaternion &normalize();

		//REMOVE ME
		//This rolls randomly, so probably not of any use. It was being used
		//by drawJoints.
		//void setRotationToPoint(const Vector &face, const Vector &point);

		//UNUSED
		//Pretty sure this is equivalent to negating either the 4th component
		//or the first three components with the provided Vector::operator-().
		//Quaternion swapHandedness();

		friend Quaternion operator*(const Quaternion &lhs, const Quaternion &rhs);

		//2020
		//Note this is 3 Quaternion multiplies (it can be done slightly more 
		//efficiently) which is more than a Matrix multiply, but maybe about
		//the same as setRotationQuaternion and multiplying.
		friend Vector operator*(const Vector &lhs, const Quaternion &rhs);
		void apply(Vector &pVec)const{ pVec = pVec * *this; }

	protected:
		//double m_val[4];
};

template<typename T>
bool float_equiv(T lhs,T rhs, double tolerance = 0.00001)
{
	return (fabs(lhs-rhs)<tolerance);
}

template<typename T>
bool floatCompareVector(const T *lhs, const T *rhs, size_t len, double tolerance = 0.00001)
{
	for(size_t index = 0; index<len; ++index)
		if(!float_equiv(lhs[index],rhs[index],tolerance))
			return false;
	return true;
}

template<typename T> T magnitude(T x1, T y1, T z1)
{
	return sqrt(x1*x1+y1*y1+z1*z1);
}

template<typename T> T distance(T x1,T y1,T z1,T x2,T y2,T z2)
{
	return magnitude(x1-x2,y1-y2,z1-z2);
}

template<typename T> T magnitude(T x1,T y1)
{
	return sqrt(x1*x1+y1*y1);
}
template<typename T> T distance(T x1,T y1,T x2,T y2)
{
	return magnitude(x1-x2,y1-y2);
}

template<typename T> T squared_mag3(const T *vec)
{
	return vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2];
}
template<typename T> T squared_mag2(const T *vec)
{
	return vec[0]*vec[0]+vec[1]*vec[1];
}

template<typename T> T mag3(const T *vec)
{
	return (T)sqrt(squared_mag3(vec));
}
template<typename T> T mag2(const T *vec)
{
	return (T)sqrt(squared_mag2(vec));
}

template<typename T> T normalize3(T *vec)
{
	T length = vec?mag3(vec):0; //???

	if(T m=length) //2022: zero divide?
	{
		m = 1/m;
		for(int t=3;t-->0;)
		vec[t]*=m;
	}
	return length;
}
template<typename T> T normalize2(T *vec)
{
	T length = vec?mag2(vec):0; //???

	if(T m=length) //2022: zero divide?
	{
		m = 1/m;
		for(int t=2;t-->0;)
		vec[t]*=m;
	}
	return length;
}

extern double distance(const Vector &v1, const Vector &v2);

extern double distance(const double v1[3], const double v2[3]);

template<typename T> T dot3(const T *lhs, const T *rhs)
{
	return lhs[0]*rhs[0]+lhs[1]*rhs[1]+lhs[2]*rhs[2];
}
template<typename T> T dot2(const T *lhs, const T *rhs)
{
	return lhs[0]*rhs[0]+lhs[1]*rhs[1];
}

static double dot_product(double *val1, double *val2)
{
	return val1[0]*val2[0]+val1[1]*val2[1]+val1[2]*val2[2];
}

template<typename T> //2020
static T *cross_product(T *result,
		const T *a, const T *b)
{
	result[0] = a[1]*b[2]-b[1]*a[2];
	result[1] = a[2]*b[0]-b[2]*a[0];
	result[2] = a[0]*b[1]-b[0]*a[1]; return result;
}

template<typename T>
static double calculate_normal(T *normal,
		const T *a, const T *b, const T *c)
{
	//Newell's Method for triangles?
	//https://github.com/zturtleman/mm3d/issues/115
	//normal[0] = a[1] *(b[2]-c[2])+b[1] *(c[2]-a[2])+c[1] *(a[2]-b[2]);
	//normal[1] = a[2] *(b[0]-c[0])+b[2] *(c[0]-a[0])+c[2] *(a[0]-b[0]);
	//normal[2] = a[0] *(b[1]-c[1])+b[0] *(c[1]-a[1])+c[0] *(a[1]-b[1]);

	T ba[3] = {b[0]-a[0],b[1]-a[1],b[2]-a[2]};
	T ca[3] = {c[0]-a[0],c[1]-a[1],c[2]-a[2]};
	cross_product(normal,ba,ca);

	return normalize3(normal);
}

template<typename T> //2020
static T *delta3(T *result, const T *a, const T *b)
{
	result[0] = a[0]-b[0];
	result[1] = a[1]-b[1];
	result[2] = a[2]-b[2]; return result;
}

#endif // __GLMATH_H
