//***********************************************************
//
// Name:		Camera.Cpp
// Last Update: 24/2/02
// Author:		Poya Manouchehri
//
// Description: CCamera holds a view and a projection matrix.
//				It also has a frustum which can be used to
//				cull objects for rendering.
//
//***********************************************************

#include "Camera.h"

CCamera::CCamera ()
{
	// set viewport to something anything should handle, but should be initialised
	// to window size before use
	m_ViewPort.m_X = 0;
	m_ViewPort.m_Y = 0;
	m_ViewPort.m_Width = 800;
	m_ViewPort.m_Height = 600;
}

CCamera::~CCamera ()
{
}
		
void CCamera::SetProjection (float nearp, float farp, float fov)
{
	float    h, w, Q;
 
	m_NearPlane = nearp;
	m_FarPlane = farp;
	m_FOV = fov;
	
	float Aspect = (float)m_ViewPort.m_Width/(float)m_ViewPort.m_Height;

    w = 1/tanf (fov*0.5f*Aspect);
	h = 1/tanf (fov*0.5f);
    Q = m_FarPlane / (m_FarPlane - m_NearPlane);
 
    m_ProjMat.SetZero ();
	m_ProjMat._11 = w;
    m_ProjMat._22 = h;
    m_ProjMat._33 = (m_FarPlane+m_NearPlane)/(m_FarPlane-m_NearPlane);;
    m_ProjMat._34 = -2*m_FarPlane*m_NearPlane/(m_FarPlane-m_NearPlane);
    m_ProjMat._43 = 1.0f;
}

//Updates the frustum planes. Should be called
//everytime the view or projection matrices are
//altered.
void CCamera::UpdateFrustum ()
{
	CMatrix3D MatFinal;
	CMatrix3D MatView;

	m_Orientation.GetInverse(MatView);
	
	MatFinal = m_ProjMat * MatView;

	//get the RIGHT plane
	m_ViewFrustum.SetNumPlanes (6);

	m_ViewFrustum.m_aPlanes[0].m_Norm.X = MatFinal._41-MatFinal._11;
	m_ViewFrustum.m_aPlanes[0].m_Norm.Y = MatFinal._42-MatFinal._12;
	m_ViewFrustum.m_aPlanes[0].m_Norm.Z = MatFinal._43-MatFinal._13;
	m_ViewFrustum.m_aPlanes[0].m_Dist	= MatFinal._44-MatFinal._14;

	//get the LEFT plane
	m_ViewFrustum.m_aPlanes[1].m_Norm.X = MatFinal._41+MatFinal._11;
	m_ViewFrustum.m_aPlanes[1].m_Norm.Y = MatFinal._42+MatFinal._12;
	m_ViewFrustum.m_aPlanes[1].m_Norm.Z = MatFinal._43+MatFinal._13;
	m_ViewFrustum.m_aPlanes[1].m_Dist	= MatFinal._44+MatFinal._14;

	//get the BOTTOM plane
	m_ViewFrustum.m_aPlanes[2].m_Norm.X = MatFinal._41+MatFinal._21;
	m_ViewFrustum.m_aPlanes[2].m_Norm.Y = MatFinal._42+MatFinal._22;
	m_ViewFrustum.m_aPlanes[2].m_Norm.Z = MatFinal._43+MatFinal._23;
	m_ViewFrustum.m_aPlanes[2].m_Dist	= MatFinal._44+MatFinal._24;

	//get the TOP plane
	m_ViewFrustum.m_aPlanes[3].m_Norm.X = MatFinal._41-MatFinal._21;
	m_ViewFrustum.m_aPlanes[3].m_Norm.Y = MatFinal._42-MatFinal._22;
	m_ViewFrustum.m_aPlanes[3].m_Norm.Z = MatFinal._43-MatFinal._23;
	m_ViewFrustum.m_aPlanes[3].m_Dist	= MatFinal._44-MatFinal._24;

	//get the FAR plane
	m_ViewFrustum.m_aPlanes[4].m_Norm.X = MatFinal._41-MatFinal._31;
	m_ViewFrustum.m_aPlanes[4].m_Norm.Y = MatFinal._42-MatFinal._32;
	m_ViewFrustum.m_aPlanes[4].m_Norm.Z = MatFinal._43-MatFinal._33;
	m_ViewFrustum.m_aPlanes[4].m_Dist	= MatFinal._44-MatFinal._34;

	//get the NEAR plane
	m_ViewFrustum.m_aPlanes[5].m_Norm.X = MatFinal._41+MatFinal._31;
	m_ViewFrustum.m_aPlanes[5].m_Norm.Y = MatFinal._42+MatFinal._32;
	m_ViewFrustum.m_aPlanes[5].m_Norm.Z = MatFinal._43+MatFinal._33;
	m_ViewFrustum.m_aPlanes[5].m_Dist	= MatFinal._44+MatFinal._34;
}

void CCamera::SetViewPort (SViewPort *viewport)
{
	m_ViewPort.m_X = viewport->m_X;
	m_ViewPort.m_Y = viewport->m_Y;
	m_ViewPort.m_Width = viewport->m_Width;
	m_ViewPort.m_Height = viewport->m_Height;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetCameraPlanePoints: return four points in camera space at given distance from camera
void CCamera::GetCameraPlanePoints(float dist,CVector3D pts[4]) const
{
	float aspect=float(m_ViewPort.m_Width)/float(m_ViewPort.m_Height);

	float x=dist*float(tan(GetFOV()*aspect*0.5));
	float y=dist*float(tan(GetFOV()*0.5));
	pts[0].X=-x;
	pts[0].Y=-y;
	pts[0].Z=dist;
	pts[1].X=x;
	pts[1].Y=-y;
	pts[1].Z=dist;
	pts[2].X=x;
	pts[2].Y=y;
	pts[2].Z=dist;
	pts[3].X=-x;
	pts[3].Y=y;
	pts[3].Z=dist;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetFrustumPoints: calculate and return the position of the 8 points of the frustum in world space
void CCamera::GetFrustumPoints(CVector3D pts[8]) const
{
	// get camera space points for near and far planes
	CVector3D cpts[8];
	GetCameraPlanePoints(m_NearPlane,pts);
	GetCameraPlanePoints(m_FarPlane,pts+4);

	// transform to world space
	for (int i=0;i<8;i++) {
		m_Orientation.Transform(cpts[i],pts[i]);
	}
}
