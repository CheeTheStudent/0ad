#ifndef _OBJECTENTRY_H
#define _OBJECTENTRY_H

class CModel;
class CSkeletonAnim;

#include <vector>
#include "CStr.h"
#include "Bound.h"
#include "ModelDef.h"

class CObjectEntry
{
public:
	struct Anim {
		// name of the animation - "Idle", "Run", etc
		CStr m_AnimName;
		// filename of the animation - manidle.psa, manrun.psa, etc
		CStr m_FileName;
		// animation speed, as specified in XML actor file
		float m_Speed;
		// the animation data, specific to the this model
		CSkeletonAnim* m_AnimData;
	};

	struct Prop {
		// name of the prop point to attach to - "Prop01", "Prop02", "Head", "LeftHand", etc ..
		CStr m_PropPointName;
		// name of the model file - art/actors/props/sword.xml or whatever
		CStr m_ModelName;
	};

public:
	CObjectEntry(int type);
	~CObjectEntry();

	bool BuildModel();

	bool Load(const char* filename);
	bool Save(const char* filename);

	// object name
	CStr m_Name;
	// texture name
	CStr m_TextureName;
	// model name
	CStr m_ModelName;
	// list of valid animations for this object
	std::vector<Anim> m_Animations;
	CSkeletonAnim* m_IdleAnim;
	CSkeletonAnim* m_WalkAnim;
	CSkeletonAnim* m_DeathAnim;
	CSkeletonAnim* m_MeleeAnim;
	CSkeletonAnim* m_RangedAnim;
	CSkeletonAnim* GetNamedAnimation( CStr animationName );
	// list of props attached to object
	std::vector<Prop> m_Props;
	// corresponding model
	CModel* m_Model;
	// type of object; index into object managers types array
	int m_Type;
};


#endif
