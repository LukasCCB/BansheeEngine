//********************************** Banshee Engine (www.banshee3d.com) **************************************************//
//**************** Copyright (c) 2016 Marko Pintera (marko.pintera@gmail.com). All rights reserved. **********************//
#include "UndoRedo/BsCmdDeleteSO.h"
#include "Scene/BsSceneObject.h"
#include "Scene/BsComponent.h"
#include "Serialization/BsMemorySerializer.h"
#include "Utility/BsUtility.h"

namespace bs
{
	CmdDeleteSO::CmdDeleteSO(const String& description, const HSceneObject& sceneObject)
		: EditorCommand(description), mSceneObject(sceneObject)
	{ }

	CmdDeleteSO::~CmdDeleteSO()
	{
		mSceneObject = nullptr;
		clear();
	}

	void CmdDeleteSO::clear()
	{
		mSerializedObjectSize = 0;
		mSerializedObjectParentId = 0;

		if (mSerializedObject != nullptr)
		{
			bs_free(mSerializedObject);
			mSerializedObject = nullptr;
		}
	}

	void CmdDeleteSO::execute(const HSceneObject& sceneObject, const String& description)
	{
		// Register command and commit it
		CmdDeleteSO* command = new (bs_alloc<CmdDeleteSO>()) CmdDeleteSO(description, sceneObject);
		SPtr<CmdDeleteSO> commandPtr = bs_shared_ptr(command);

		UndoRedo::instance().registerCommand(commandPtr);
		commandPtr->commit();
	}

	void CmdDeleteSO::commit()
	{
		clear();

		if (mSceneObject == nullptr || mSceneObject.isDestroyed())
			return;

		recordSO(mSceneObject);
		mSceneObject->destroy();
	}

	void CmdDeleteSO::revert()
	{
		HSceneObject parent;
		if (mSerializedObjectParentId != 0)
			parent = static_object_cast<SceneObject>(GameObjectManager::instance().getObject(mSerializedObjectParentId));

		CoreSerializationContext serzContext;
		serzContext.goState = bs_shared_ptr_new<GameObjectDeserializationState>(GODM_RestoreExternal | GODM_UseNewIds);

		// Object might still only be queued for destruction, but we need to fully destroy it since we're about to replace
		// the potentially only reference to the old object
		if (!mSceneObject.isDestroyed())
			mSceneObject->destroy(true);

		MemorySerializer serializer;
		SPtr<SceneObject> restored = std::static_pointer_cast<SceneObject>(
			serializer.decode(mSerializedObject, mSerializedObjectSize, &serzContext));

		EditorUtility::restoreIds(restored->getHandle(), mSceneObjectProxy);
		restored->setParent(parent);

		restored->_instantiate();
		mSceneObject = restored->getHandle();
	}

	void CmdDeleteSO::recordSO(const HSceneObject& sceneObject)
	{
		bool isInstantiated = !sceneObject->hasFlag(SOF_DontInstantiate);
		sceneObject->_setFlags(SOF_DontInstantiate);

		MemorySerializer serializer;
		mSerializedObject = serializer.encode(sceneObject.get(), mSerializedObjectSize);

		if (isInstantiated)
			sceneObject->_unsetFlags(SOF_DontInstantiate);

		HSceneObject parent = sceneObject->getParent();
		if (parent != nullptr)
			mSerializedObjectParentId = parent->getInstanceId();

		mSceneObjectProxy = EditorUtility::createProxy(sceneObject);
	}
}
