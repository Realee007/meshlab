/****************************************************************************
* VCGLib                                                            o o     *
* Visual and Computer Graphics Library                            o     o   *
*                                                                _   O  _   *
* Copyright(C) 2004                                                \/)\/    *
* Visual Computing Lab                                            /\/|      *
* ISTI - Italian National Research Council                           |      *
*                                                                    \      *
* All rights reserved.                                                      *
*                                                                           *
* This program is free software; you can redistribute it and/or modify      *
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 2 of the License, or         *
* (at your option) any later version.                                       *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
*                                                                           *
****************************************************************************/

#ifndef HOLESETMANAGER_H
#define HOLESETMANAGER_H

#include "fgtHole.h"
#include "fgtBridge.h"
#include "vcg/complex/trimesh/hole.h"
#include <vcg/container/simple_temporary_data.h>


/*  HoleSetManager class rappresent an entity which manages the holes founded
 *  into the same MESH.
 *  It allows to invoke some functionality for each (may be the selected ones)
 *  holes as filling and bridging.
 *  His presence is necessary because it connect holes to a mesh with the
 *  additional data used by holes to mark its "characteristic" faces.
 */
template <class MESH>
class HoleSetManager
{
	enum FaceAttribute
  {
    NONE      = 0x0000,
    BORDER		= 0x0001,
    PATCH			= 0x0002,
    COMPENET  = 0x0004,
    BRIDGE		= 0x0020
  };

	typedef FgtHole< MESH >                                   HoleType;
	typedef typename std::vector< HoleType >    							HoleVector;
	typedef typename HoleVector::iterator											HoleIterator;
	typedef typename MESH::FaceType														FaceType;
	typedef typename MESH::FacePointer												FacePointer;
	typedef typename MESH::FaceContainer											FaceContainer;
	typedef typename vcg::tri::Hole<MESH>											vcgHole;
	typedef typename vcgHole::Info														HoleInfo;
	typedef typename vcg::face::Pos<FaceType>									PosType;
	typedef typename vcg::SimpleTempData<FaceContainer, int>	SimpleData;
	typedef typename HoleType::FillerMode								FillerMode;

public:

	HoleSetManager()
	{
		autoBridgeCB = 0;
	};

	void Init(MESH* m)
	{
		nAccepted = nSelected = 0;
		mesh = m;
		faceAttr = new SimpleData (m->face, NONE);
		getMeshHoles();
	};

	void Clear()
	{
		typename HoleVector::iterator it;
		for(it=holes.begin(); it!=holes.end(); it++)
			it->ResetFlag();

		holes.clear();
	};

 /***************** PerFace additional data ********************/

	inline bool IsHoleBorderFace(FacePointer f) const { return ( (*faceAttr)[f] & BORDER) != 0; };
	inline bool IsPatchFace(FacePointer f)			const { return ((*faceAttr)[f] & PATCH) != 0; };
	inline bool IsCompFace(FacePointer f)				const { return ((*faceAttr)[f] & COMPENET) != 0; };
	inline bool IsBridgeFace(FacePointer f)			const { return ((*faceAttr)[f] & BRIDGE) != 0; };

	inline void SetHoleBorderAttr(FacePointer f) {	(*faceAttr)[f] |= BORDER; };
	inline void ClearHoleBorderAttr(FacePointer f) { (*faceAttr)[f] &= (~BORDER); };

	inline void SetPatchAttr(FacePointer f) { (*faceAttr)[f] |= PATCH; };
	inline void ClearPatchAttr(FacePointer f) { (*faceAttr)[f] &= (~PATCH);	}

	inline void SetCompAttr(FacePointer f) { (*faceAttr)[f] |= COMPENET; };
	inline void ClearCompAttr(FacePointer f) { (*faceAttr)[f] &= (~COMPENET); };

	inline void SetBridgeAttr(FacePointer f){ (*faceAttr)[f] |= BRIDGE; };
	inline void ClearBridgeAttr(FacePointer f){ (*faceAttr)[f] &= (~BRIDGE);	};

	/**************** Statistical info *****************/
  inline int SelectionCount() const { return nSelected; };
  inline int HolesCount()			const { return holes.size(); };
  inline int AcceptedCount()	const { return nAccepted; };

	/**************** Holes editing ********************/

	bool Fill(FillerMode mode)
	{
		if(nSelected==0)
			return false;

		std::vector<FacePointer *> local_facePointer;
		HoleType::AddFaceReference(holes, local_facePointer);

		HoleIterator it = holes.begin();
		for( ; it != holes.end(); it++ )
			if( it->IsSelected() )
				it->Fill(mode, *mesh, local_facePointer);

		nAccepted=nSelected;
		return true;
	};

	void ConfirmFilling(bool accept)
	{
		nSelected=0;
		HoleIterator it = holes.begin();
		while( it != holes.end() )
		{
			if( it->IsFilled() )
			{
				if( (it->IsSelected() && !it->IsAccepted()) || !accept)
				{
					if( it->IsFilled() )
					{
				  		it->RestoreHole();
				  		nSelected++;
					}
				}
				else if( it->IsSelected() && it->IsAccepted() )
				{
					it->ResetFlag();
					it = holes.erase(it);
					continue;
				}
			}
			it++;
		}
		countSelected();
	};

	inline void ConfirmBridges()
	{
		FgtBridge<CMeshO>::AcceptBridges(this);
	};

	inline void DiscardBridges()
	{
		FgtBridge<CMeshO>::RemoveBridges(this);
		countSelected();
	};

	bool CloseNonManifoldHoles()
	{
		return (FgtBridge<CMeshO>::CloseNonManifoldVertex(this) >0);
	};

	int AutoSelfBridging(double distCoeff, std::vector<FacePointer*> *facesRef)
	{
		int n = FgtBridge<CMeshO>::AutoSelfBridging(this, distCoeff, facesRef);
		countSelected();
		return n;
	};

	int AutoMultiBridging(double distCoeff, std::vector<FacePointer*> *facesRef)
	{
		int n = FgtBridge<CMeshO>::AutoMultiBridging(this, distCoeff, facesRef);
		countSelected();
		return n;
	};

	/** Return index of hole adjacent to picked face into holes vector.
	 *  Also return the iterator on correct position in holes list.
	 */
	int FindHoleFromFace(FacePointer bFace, HoleIterator &it)
	{
		int index = 0;

		// it know if bFace is adjacent to patchFace
		FacePointer patchF = 0;
		if(IsPatchFace(bFace))
			patchF = bFace;
		else
		{
			for( int i=0; i<3; i++)
				if(IsPatchFace(bFace->FFp(i)) && !IsBridgeFace(bFace->FFp(i)))
					patchF = bFace->FFp(i);
		}

		HoleIterator hit = holes.begin();
		if(patchF == 0)
		{
			if(IsHoleBorderFace(bFace))
			{
				// border face belong to an hole not filled... it can walk on border
				for( ; hit != holes.end(); ++hit)
				{
					if(!hit->IsFilled())
						if(hit->HaveBorderFace(bFace))
						{
							it = hit;
							return index;
						}
					index++;
				}
			}
		}
		else
		{
			// bFace belong filled hole, adjF is its patch
			assert(IsPatchFace(patchF));
			HoleIterator hit = holes.begin();
			for( ; hit != holes.end(); ++hit)
			{
				// for each hole check if face is its border face
				if(hit->IsFilled())
					if(hit->HavePatchFace(patchF))
					{
						it = hit;
						return index;
					}
				index++;
			}
		}
		it = holes.end();	// invalid iterator
		return -1;
	};

	/** Return boolean indicatind if the picking have select a border face which can be used
	 *  as abutment for a bridge. If true it also return BridgeAbutment allowing to know
	 *  border edge and its relative hole.
	 */
	bool FindBridgeAbutmentFromPick( FacePointer bFace, int pickedX, int pickedY,
																					BridgeAbutment<MESH> &pickedResult)
	{
		if( vcg::face::BorderCount(*bFace) == 0 )
			return false;

		HoleIterator hit;
		if( FindHoleFromFace(bFace, hit) < 0 )
		{
			pickedResult.SetNull();
			return false;
		}

		pickedResult.h = &*hit;
		pickedResult.f = bFace;
		if( vcg::face::BorderCount(*bFace) == 1 )
		{
			// it choose the only one border edge
			for(int i=0; i<3; i++)
				if(vcg::face::IsBorder<FaceType>(*bFace, i))
					pickedResult.z = i;
		}
		else
		{
			// looking for the closest border edge to pick point
			PosType retPos = getClosestPos(bFace, pickedX, pickedY);
			pickedResult.f = retPos.f;
			pickedResult.z = retPos.z;
		}

		return true;
	};



private:

	/* Inspect a mesh to find its holes. */
	void getMeshHoles()
	{
		holes.clear();
		std::vector<HoleInfo> vhi;

		//prendo la lista di info(sugli hole) tutte le facce anche le non selezionate
		FaceType::NewBitFlag();
		int borderFlag = vcgHole::GetInfo(*mesh, false, vhi);

		HoleType::ResetHoleId();
		typename std::vector<HoleInfo>::iterator itH = vhi.begin();
		for( ; itH != vhi.end(); itH++)
		{
			holes.push_back(HoleType(*itH, QString("Hole_%1").arg(HoleType::GetHoleId(),3,10,QChar('0')), this));

			// reset flag and setting of
			PosType curPos = itH->p;
			do{
				curPos.f->ClearUserBit(borderFlag);
				SetHoleBorderAttr(curPos.f);
				curPos.NextB();
				assert(curPos.IsBorder());
			}while( curPos != itH->p );
		}

		FaceType::DeleteBitFlag(borderFlag);
	};

	/*  Return border half-edge of the same face which is nearest to point(x, y) of glArea.
	 */
	static PosType getClosestPos(FaceType* face, int x, int y)
	{
		double mvmatrix[16], projmatrix[16];
		GLint viewport[4];
		double rx, ry, rz;
		vcg::Point2d vertex[3];
		vcg::Point2d point((double)x, (double)y);

		glGetDoublev (GL_MODELVIEW_MATRIX, mvmatrix);
		glGetDoublev (GL_PROJECTION_MATRIX, projmatrix);
		glGetIntegerv(GL_VIEWPORT, viewport);

		for(int i=0; i<3; i++)
		{
			gluProject(face->V(i)->P()[0],face->V(i)->P()[1],face->V(i)->P()[2],mvmatrix,projmatrix,viewport,&rx,&ry,&rz);
			vertex[i] = vcg::Point2d(rx,ry);
		}

		double dist = DBL_MAX;
		int nearest = 0;
		for(int i=0; i<3; i++)
		{
			if(!vcg::face::IsBorder<FaceType>(*face, i))
				continue;

			vcg::Line2d el(vertex[i], vertex[(i+1)%3]-vertex[i]);
			double tmp = vcg::Distance(el, point);

			if(dist > tmp)
			{
				dist = tmp;
				nearest = i;
			}
		}
		return PosType(face, nearest, face->V(nearest) );
	};


	void countSelected()
	{
		nSelected = 0;
		HoleIterator hit = holes.begin();
		for( ; hit!=holes.end(); hit++)
			if(hit->IsSelected())
				nSelected++;
	};


public:
	int nSelected;
	int nAccepted;
	MESH* mesh;
	HoleVector holes;
	SimpleData* faceAttr;

	AutoBridgingCallback* autoBridgeCB;
};

#endif
